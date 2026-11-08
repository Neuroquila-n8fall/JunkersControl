#include <Arduino.h>
#include <telnet.h>
#include <WiFi.h>
#include <errno.h>
#include <lwip/sockets.h>
#include <logger.h>
#include <configuration.h>

const uint TelnetServerPort = 23;
WiFiServer TelnetServer(TelnetServerPort);

namespace
{
  // Transient output queue: producers (any task) append whole lines, the main loop drains it.
  constexpr size_t QueueSize = 2048;
  constexpr size_t CommandMax = 32;

  uint8_t queue[QueueSize];
  size_t queueHead = 0;
  size_t queueLength = 0;
  uint32_t droppedLines = 0;
  // Set and cleared by the main loop only; producers read it to skip work when nobody listens.
  volatile bool clientActive = false;
  // Main loop only; its socket is never touched from other tasks.
  WiFiClient TelnetRemoteClient;
  StaticSemaphore_t queueLockStorage;
  SemaphoreHandle_t queueLock = xSemaphoreCreateMutexStatic(&queueLockStorage);

  // Telnet command bytes (RFC 854)
  constexpr uint8_t IacByte = 255;
  constexpr uint8_t SubnegotiationBegin = 250;
  constexpr uint8_t SubnegotiationEnd = 240;

  enum ParseState : uint8_t
  {
    Data,
    Iac,
    IacOption,
    Subnegotiation,
    SubnegotiationIac
  };
  ParseState parseState = Data;
  char command[CommandMax];
  size_t commandLength = 0;
  bool commandOverflow = false;

  void appendLocked(const char *data, size_t length)
  {
    size_t tail = (queueHead + queueLength) % QueueSize;
    size_t first = min(length, QueueSize - tail);
    memcpy(queue + tail, data, first);
    memcpy(queue, data + first, length - first);
    queueLength += length;
  }

  // Announces lines lost to a full queue, as soon as the notice fits.
  void noteDroppedLocked()
  {
    if (droppedLines == 0)
      return;
    char note[40];
    int length = snprintf(note, sizeof note, "… %u lines dropped\r\n", (unsigned)droppedLines);
    if (length > 0 && (size_t)length <= QueueSize - queueLength)
    {
      appendLocked(note, length);
      droppedLines = 0;
    }
  }

  void closeClient()
  {
    clientActive = false;
    xSemaphoreTake(queueLock, portMAX_DELAY);
    queueHead = 0;
    queueLength = 0;
    droppedLines = 0;
    xSemaphoreGive(queueLock);
    TelnetRemoteClient.stop();
  }

  void execute()
  {
    while (commandLength > 0 && command[commandLength - 1] == ' ')
      commandLength--;
    command[commandLength] = '\0';

    if (strcmp(command, "reboot") == 0)
    {
      LogWarn("Reboot in 5 seconds (telnet command)");
      // The delay below stops the main loop, so push the message to telnet and SSE first.
      LogService();
      delay(5000);
      ESP.restart();
    }
  }

  void feed(uint8_t b)
  {
    switch (parseState)
    {
    case Iac:
      if (b >= 251 && b <= 254) // WILL/WONT/DO/DONT carry one option byte
        parseState = IacOption;
      else if (b == SubnegotiationBegin)
        parseState = Subnegotiation;
      else
        parseState = Data; // escaped 0xFF, NOP, ...
      return;
    case IacOption:
      parseState = Data;
      return;
    case Subnegotiation:
      if (b == IacByte)
        parseState = SubnegotiationIac;
      return;
    case SubnegotiationIac:
      parseState = (b == SubnegotiationEnd) ? Data : Subnegotiation;
      return;
    default:
      break;
    }

    if (b == IacByte)
    {
      parseState = Iac;
    }
    else if (b == '\r' || b == '\n')
    {
      if (commandLength > 0 && !commandOverflow)
        execute();
      commandLength = 0;
      commandOverflow = false;
    }
    else if (b == 8 || b == 127) // backspace / delete
    {
      if (commandLength > 0)
        commandLength--;
    }
    else if (b >= 32 && b <= 126 && !(b == ' ' && commandLength == 0))
    {
      if (commandLength < sizeof command - 1)
        command[commandLength++] = tolower(b);
      else
        commandOverflow = true;
    }
  }
}

bool TelnetWrite(const char *data, size_t length)
{
  if (!clientActive)
    return false;
  bool queued = false;
  xSemaphoreTake(queueLock, portMAX_DELAY);
  if (clientActive)
  {
    noteDroppedLocked();
    if (length <= QueueSize - queueLength)
    {
      appendLocked(data, length);
      queued = true;
    }
    else
    {
      droppedLines++;
    }
  }
  xSemaphoreGive(queueLock);
  return queued;
}

void TelnetFlush()
{
  if (!clientActive)
    return;
  // The main loop is the only consumer and the only place the queue is reset, and producers
  // write only into free space, so the snapshot stays valid while the lock is released for lwIP.
  size_t head, length;
  xSemaphoreTake(queueLock, portMAX_DELAY);
  if (queueLength == 0)
    noteDroppedLocked();
  head = queueHead;
  length = queueLength;
  xSemaphoreGive(queueLock);

  size_t consumed = 0;
  bool failed = false;
  // At most two passes: the queue wraps once.
  while (consumed < length)
  {
    size_t chunk = min(length - consumed, QueueSize - head);
    ssize_t sent = send(TelnetRemoteClient.fd(), queue + head, chunk, MSG_DONTWAIT);
    if (sent <= 0)
    {
      // A full send buffer is normal on a slow link; anything else means the socket is gone.
      failed = sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOMEM);
      break;
    }
    head = (head + sent) % QueueSize;
    consumed += sent;
    if ((size_t)sent < chunk)
      break;
  }
  if (consumed > 0)
  {
    xSemaphoreTake(queueLock, portMAX_DELAY);
    queueHead = (queueHead + consumed) % QueueSize;
    queueLength -= consumed;
    xSemaphoreGive(queueLock);
  }
  if (failed)
    closeClient();
}

//Checks for new Telnet connections
void CheckForConnections()
{
  if (clientActive && !TelnetRemoteClient.connected())
  {
    closeClient();
    LogInfo("Telnet client disconnected");
  }

  //Check if we're having a new client connection
  if (!TelnetServer.hasClient())
    return;

  //Check if we already have a client
  if (clientActive)
  {
    //Reject connection.
    WiFiClient rejectedClient = TelnetServer.accept();
    rejectedClient.stop();
    LogInfo("Telnet connection rejected");
    return;
  }

  TelnetRemoteClient = TelnetServer.accept();
  parseState = Data;
  commandLength = 0;
  commandOverflow = false;
  clientActive = true;

  IPAddress ip = WiFi.localIP();
  char greeting[320];
  int length = snprintf(greeting, sizeof greeting,
                        "——————————————————————————\r\nYou are connected to: %.64s (%u.%u.%u.%u)\r\n——————————————————————————\r\n",
                        configuration.Wifi.Hostname, (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
  if (length > 0 && (size_t)length < sizeof greeting)
    TelnetWrite(greeting, length);
  LogInfo("Telnet connection accepted");
}

//Read commands from Telnet clients without blocking: a few bytes per call, parsed incrementally.
void ReadFromTelnet()
{
  if (!clientActive)
    return;
  int available = TelnetRemoteClient.available();
  if (available <= 0)
    return;
  uint8_t chunk[32];
  int count = TelnetRemoteClient.read(chunk, min(available, (int)sizeof chunk));
  for (int i = 0; i < count; i++)
    feed(chunk[i]);
}

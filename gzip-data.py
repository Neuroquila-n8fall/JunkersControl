# SCRIPT TO GZIP CRITICAL FILES FOR ACCELERATED WEBSERVING
# Credit where credit is due since I am absolutely the worst at python:
# see also https://community.platformio.org/t/question-esp32-compress-files-in-data-to-gzip-before-upload-possible-to-spiffs/6274/10
# Snacked from https://community.platformio.org/t/question-esp32-compress-files-in-data-to-gzip-before-upload-possible-to-spiffs/6274/11
# ...but slightly customized to fit our needs here.
#

Import( 'env', 'projenv' )

import os
import gzip
import shutil
import glob
import pathlib

# HELPER TO GZIP A FILE
def gzip_file( src_path, dst_path ):
    with open( src_path, 'rb' ) as src, gzip.open( dst_path, 'wb' ) as dst:
        for chunk in iter( lambda: src.read(4096), b"" ):
            dst.write( chunk )

# GZIP DEFINED FILES FROM 'data' DIR to 'data/gzip/' DIR
def gzip_webfiles( source, target, env ):
    
    # FILETYPES / SUFFIXES WHICh NEED TO BE GZIPPED
    filetypes_to_gzip = [ '.css', '.html', '.js' ]

    print( '\nGZIP: INITIATED GZIP FOR SPIFFS...\n' )
    SOURCE_DIR_NAME = 'data'
    TARGET_DIR_NAME = 'data_build'

    source_data_path = os.path.join( env.get('PROJECT_DIR'), SOURCE_DIR_NAME )
    target_data_path = os.path.join( env.get( 'PROJECT_DIR' ), TARGET_DIR_NAME )

    # CHECK DATA DIR
    if not os.path.exists( source_data_path ):
        print( 'GZIP: DATA DIRECTORY MISSING AT PATH: ' + source_data_path )
        print( 'GZIP: PLEASE CREATE THE DIRECTORY FIRST (ABORTING)' )
        print( 'GZIP: FAILURE / ABORTED' )
        return
    
    # CHECK GZIP DIR
    if not os.path.exists( target_data_path ):
        print( 'GZIP: GZIP DIRECTORY MISSING AT PATH: ' + target_data_path )
        print( 'GZIP: TRYING TO CREATE IT...' )
        try:
            os.mkdir( target_data_path )
        except Exception as e:
            print( 'GZIP: FAILED TO CREATE DIRECTORY: ' + target_data_path )
            # print( 'GZIP: EXCEPTION... ' + str( e ) )
            print( 'GZIP: PLEASE CREATE THE DIRECTORY FIRST (ABORTING)' )
            print( 'GZIP: FAILURE / ABORTED' )
            return

    files_to_gzip = []
    files_to_copy = []

    for dirpath, dirs, files in os.walk(source_data_path):	
        for filename in files:
            fileFullPath = os.path.join(dirpath, filename)
            ext = pathlib.Path( fileFullPath ).suffix.lower()
            # Create the directories necessary
            target_file_name = fileFullPath.replace(source_data_path, target_data_path)
            parentDir = pathlib.Path(target_file_name).parent
            parentDir.mkdir( parents=True, exist_ok=True )
            if ext in filetypes_to_gzip:
                # GZIP
                files_to_gzip.append( fileFullPath )
            else:
                # Just Copy
                files_to_copy.append( fileFullPath )

    # Copy files not included in gzip extension list
    for file in files_to_copy:        
        # Just replace the path portion of our file while keeping the rest of the structure as-is
        targetFileName = file.replace(source_data_path, target_data_path)   
        print('  Copying file: ' + file + ' to ', targetFileName)
        if os.path.exists(targetFileName):
            print('File exists. Removing ' + targetFileName)
            os.remove(targetFileName)        
        shutil.copy(file, targetFileName)
    
    print( 'GZIP: GZIPPING FILES... {}\n'.format( files_to_gzip ) )

    # COMPRESS AND MOVE FILES
    was_error = False
    try:
        for source_file_path in files_to_gzip:
            print( 'GZIP: ZIPPING... ' + source_file_path )
            target_file_path = source_file_path.replace(source_data_path, target_data_path) + '.gz'       
            # CHECK IF FILE ALREADY EXISTS
            if os.path.exists( target_file_path ):
                print( 'GZIP: REMOVING... ' + target_file_path )
                os.remove( target_file_path )
            #print( 'GZIP: GZIPPING FILE...\n' + source_file_path + ' TO...\n' + target_file_path + "\n\n" )
            print( 'GZIP: GZIPPED... ' + target_file_path + "\n" )
            gzip_file( source_file_path, target_file_path )
    except IOError as e:
        was_error = True
        print( 'GZIP: FAILED TO COMPRESS FILE: ' + source_file_path )
        print( 'GZIP: EXCEPTION... {}'.format( e ) )
    if was_error:
        print( 'GZIP: FAILURE/INCOMPLETE.\n' )
    else:
        print( 'GZIP: SUCCESS/COMPRESSED.\n' )

# IMPORTANT, this needs to be added to call the routine
env.AddPreAction( '$BUILD_DIR/littlefs.bin', gzip_webfiles )
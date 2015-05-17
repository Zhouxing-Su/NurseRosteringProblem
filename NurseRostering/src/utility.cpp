#include "utility.h"


using namespace std;


//
std::string getLastNoneEmptyLine( const std::string &filename )
{
    std::string lastLine;
    std::ifstream fin( filename );

    fin.seekg( 0, std::ios_base::end );
    char ch;
    // leave out last '\r' or '\n'
    do {
        fin.seekg( -1, std::ios_base::cur );
        if (fin.tellg() <= 0) {
            fin.seekg( 0 );
            break;
        }
        ch = static_cast<char>(fin.peek());
    } while ((ch == '\n') || (ch == '\r'));

    do {
        fin.seekg( -1, std::ios_base::cur );
        if (fin.tellg() <= 0) {
            fin.seekg( 0 );
            break;
        }
        ch = static_cast<char>(fin.peek());
    } while ((ch != '\n') && (ch != '\r'));

    fin.get();
    std::getline( fin, lastLine );
    fin.close();

    return lastLine;
}

//
std::string getTime()
{
    char timeBuf[64];
    time_t t = time( NULL );
    tm *date = localtime( &t );
    strftime( timeBuf, 64, "%Y-%m-%d %a %H:%M:%S", date );

    return std::string( timeBuf );
}

//
void errorLog( const std::string &msg )
{
#ifdef INRC2_LOG
    cerr << getTime() << "," << msg << endl;
#endif
}

//
const std::string FileLock::LockName( ".lock" );
const std::ios_base::openmode FileLock::ReadMode( ios::in | ios::binary );
const std::ios_base::openmode FileLock::WriteMode( ios::out | ios::binary );

void FileLock::unlock( const std::string &filename )
{
    remove( (filename + LockName).c_str() );
}

FileLock::FileLock( const string &filename, unsigned id )
    : lockFileName( filename + LockName ),
    signature( (static_cast<long long>(std::random_device()()) << BIT_NUM_OF_INT) | id ),
    retryInterval( TRY_LOCK_INTERVAL + (signature % TRY_LOCK_INTERVAL) )
{
}

bool FileLock::checkLock() const
{
    long long sign = 0;
    fstream lockFile( lockFileName, ReadMode );
    if (lockFile.is_open()) {
        lockFile.read( reinterpret_cast<char*>(&sign), sizeof( sign ) );
        lockFile.close();
    }

    return (sign == signature);
}

void FileLock::tryLock()
{
    fstream lockFile( lockFileName, ReadMode );
    if (lockFile.is_open()) {
        lockFile.close();
    } else {
        lockFile.open( lockFileName, WriteMode );
        lockFile.clear();
        lockFile.write( reinterpret_cast<const char*>(&signature), sizeof( signature ) );
        lockFile.close();
    }
}

void FileLock::lock()
{
    do {
        tryLock();  // result can be 1.success; 2.fail; 3.share lock
        // wait in case others write signature right after checkLock
        this_thread::sleep_for( chrono::milliseconds( retryInterval + (rand() % TRY_LOCK_INTERVAL) ) );
    } while (!checkLock());
}

void FileLock::unlock()
{
    remove( lockFileName.c_str() );
}


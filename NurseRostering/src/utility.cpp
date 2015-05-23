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
mutex logFileMutex;

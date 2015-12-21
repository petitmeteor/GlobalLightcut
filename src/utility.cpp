#include "utility.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <string>

namespace utility
{

std::string read_file( const char *fname )
{
    std::ifstream file( fname, std::ios::in | std::ios::binary | std::ios::ate );
    if( !file.is_open() )
        throw std::runtime_error( utility::sprintfpp( "Unable to open file %s", fname ).c_str() );

    std::string text;
    text.resize( file.tellg() );
    file.seekg( 0 );
    file.read( &text[0], text.size() );
    return text;
}

//Ned note: got from http://stackoverflow.com/a/8098080, CC-BY-SA
std::string sprintfpp( const char *fmt_str, ... )
{
    int final_n, n = ( ( int )std::strlen( fmt_str ) ) * 2; /* Reserve two times as much as the length of the fmt_str */
    std::string str;
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while( 1 )
    {
        formatted.reset( new char[n] ); /* Wrap the plain char array into the unique_ptr */
        strcpy( &formatted[0], fmt_str );
        va_start( ap, fmt_str );
        final_n = vsnprintf( &formatted[0], n, fmt_str, ap );
        va_end( ap );
        if( final_n < 0 || final_n >= n )
            n += abs( final_n - n + 1 );
        else
            break;
    }
    return std::string( formatted.get() );
}

}
#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <cstdlib>
#include <cstdarg>
#include <memory>
#include <string>
#include <GL/glew.h>

namespace utility
{

std::string read_file( const char *fname );
std::string sprintfpp( const char *fmt_str, ... );

namespace _scope_exit_detail
{

//C++11-style ScopeExit
template <typename ExitFuncType>
struct scope_guard
{
    scope_guard( ExitFuncType f ) : f( f ), run( true ) {}
    scope_guard( const scope_guard &rhs ) = delete;
    scope_guard( scope_guard &&rhs ) : f( rhs.f ), run( true )
    {
        rhs.run = false;
    }
    ~scope_guard()
    {
        if( run ) f();
    }
    scope_guard &operator=( const scope_guard &rhs ) = delete;
    scope_guard &operator=( scope_guard &&rhs )
    {
        f = rhs.f;
        rhs.run = false;
        run = true;
    }
    ExitFuncType f;
    bool run;
};
#define SCOPE_EXIT_STRJOIN(a, b) SCOPE_EXIT_STRJOIN_INDIRECT(a, b)
#define SCOPE_EXIT_STRJOIN_INDIRECT(a, b) a ## b
}

template <typename ExitFuncType>
_scope_exit_detail::scope_guard<ExitFuncType> scope_exit( ExitFuncType f )
{
    return _scope_exit_detail::scope_guard<ExitFuncType> ( f );
};
#define SCOPE_EXIT(f) auto SCOPE_EXIT_STRJOIN(_scope_exit_, __LINE__) = utility::scope_exit(f)

}

#endif

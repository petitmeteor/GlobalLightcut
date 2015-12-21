//Configuration macros:
//	GLS_NO_BIND_CHECK: no bind checking in debug mode
//

#ifndef _GL_SNIPPETS_H_
#define _GL_SNIPPETS_H_

#include <string>
#include <GL/glew.h>
#include <utility.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace gls
{

namespace _gls_detail
{

template<typename T>
inline void check_bound( const T &obj )
{
#	if defined(_DEBUG) && !defined(GLS_NO_BIND_CHECK)
    GLint ret;
    if( obj.get_bind_parameter_name() == 0 )
        return;
    glGetIntegerv( obj.get_bind_parameter_name(), &ret );
    assert( obj.get() == ret );
#	endif
}

}

class shader
{
public:
    //common interfaces
    GLuint get() const
    {
        return shader_;
    }

public:
    shader() : shader_( 0 ) {}
    shader( const char *path, GLenum type ): path_( path ), type_( type )
    {
        std::string shader_source = utility::read_file( path );

        shader_ = glCreateShader( type );
        {
            const char *shader_source_ptr = shader_source.c_str();
            GLint shader_source_length = GLint( shader_source.size() );
            glShaderSource( shader_, 1, &shader_source_ptr, &shader_source_length );
        }
        glCompileShader( shader_ );

        //check failure
        {
            GLint compiled;
            glGetShaderiv( shader_, GL_COMPILE_STATUS, &compiled );

            if( !compiled )
            {
                //fetch error log
                std::string infolog;
                {
                    GLint info_log_len;
                    glGetShaderiv( shader_, GL_INFO_LOG_LENGTH, &info_log_len );
                    if( info_log_len > 0 )
                    {
                        infolog.resize( info_log_len );
                        glGetShaderInfoLog( shader_, info_log_len, nullptr, &infolog[0] );
                    }
                }

                throw std::runtime_error(
                    utility::sprintfpp(
                        "shader (%s) compilation failure; reason: %s",
                        path,
                        infolog.c_str()
                    ).c_str() );
            }
        }
    }
    shader( const shader & ) = delete;
    shader &operator=( const shader & ) = delete;
    virtual ~shader()
    {
        if( shader_ )
            glDeleteShader( shader_ );
    }
    shader( shader &&rhs ): shader_( 0 )
    {
        *this = std::move( rhs );
    }
    shader &operator=( shader &&rhs )
    {
        if( shader_ )
            glDeleteShader( shader_ );

        shader_ = rhs.shader_;
        rhs.shader_ = 0;
        type_ = rhs.type_;
        path_ = std::move( rhs.path_ );
        return *this;
    }
private:
    GLuint shader_;
    GLenum type_;
    std::string path_;
};

struct _program_definition
{
    const char *vertex_shader_path;
    const char *fragment_shader_path;
    std::initializer_list<const char *> attribute_names;
    std::initializer_list<const char *> frag_data_names;
    std::initializer_list<const char *> uniform_names;
};

#define GLS_PROGRAM_DEFINE(name, ...) \
	const ::gls::_program_definition name = {__VA_ARGS__}

class program
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        return GL_CURRENT_PROGRAM;
    }
    GLuint get() const
    {
        return program_;
    }
    void bind()
    {
        glUseProgram( program_ );
    }
    void unbind()
    {
        glUseProgram( 0 );
    }

public:
    program() : program_( 0 ) {}
    program( const _program_definition &pdef ) : program( pdef.vertex_shader_path, pdef.fragment_shader_path, pdef.attribute_names, pdef.frag_data_names, pdef.uniform_names ) {}

    template<class AttributeNameContainerType, class FragDataNameContainerType, class UniformNameContainerType>
    program(
        const char *vertex_shader_path,
        const char *fragment_shader_path,
        AttributeNameContainerType attribute_names,
        FragDataNameContainerType frag_data_names,
        UniformNameContainerType uniform_names
    )
    {
        program_ = glCreateProgram();

        set_attribute_map( attribute_names );
        set_frag_data_map( frag_data_names );


        shaders_.push_back( shader( vertex_shader_path, GL_VERTEX_SHADER ) );
        shaders_.push_back( shader( fragment_shader_path, GL_FRAGMENT_SHADER ) );
        for( auto it = std::begin( shaders_ ); it != std::end( shaders_ ); ++it )
            glAttachShader( program_, it->get() );
        glLinkProgram( program_ );

        //check failure
        {
            GLint linked;
            glGetProgramiv( program_, GL_LINK_STATUS, &linked );

            if( !linked )
            {
                //fetch error log
                std::string infolog;
                {
                    GLint info_log_len;
                    glGetProgramiv( program_, GL_INFO_LOG_LENGTH, &info_log_len );
                    if( info_log_len > 0 )
                    {
                        infolog.resize( info_log_len );
                        glGetProgramInfoLog( program_, info_log_len, nullptr, &infolog[0] );
                    }
                }

                throw std::runtime_error(
                    utility::sprintfpp(
                        "program link failure; reason: %s",
                        infolog.c_str()
                    ).c_str() );
            }
        }
        set_uniform_map( uniform_names );
    }

    template<class AttributeNameContainerType>
    void set_attribute_map( AttributeNameContainerType attribute_names )
    {
        GLuint i = 0;
        for( auto it = std::begin( attribute_names ); it != std::end( attribute_names ); ++it )
            glBindAttribLocation( program_, i++, *it );
    }

    template<class FragDataNameContainerType>
    void set_frag_data_map( FragDataNameContainerType frag_data_names )
    {
        GLuint i = 0;
        for( auto it = std::begin( frag_data_names ); it != std::end( frag_data_names ); ++it )
            glBindFragDataLocation( program_, i++, *it );
    }

    template<class UniformNameContainerType>
    void set_uniform_map( UniformNameContainerType uniform_names )
    {
        uniform_locations_.clear();
        for( auto it = std::begin( uniform_names ); it != std::end( uniform_names ); ++it )
            uniform_locations_.push_back( glGetUniformLocation( program_, *it ) );
    }
    template<class ValueType>
    void set_uniform( GLuint index, const ValueType &value );

    template<typename... ArgsType>
    void set_uniforms( ArgsType &&... args )
    {
        set_uniforms_from( 0, std::forward<ArgsType>( args )... );
    }

    template<typename... ArgsType>
    void set_uniforms_from( GLuint index_from, ArgsType &&... args )
    {
        set_uniforms_from_impl_<ArgsType...>::run( *this, index_from, std::forward<ArgsType>( args )... );
    }

    program( const program & ) = delete;
    program &operator=( const program & ) = delete;
    virtual ~program()
    {
        if( program_ )
            glDeleteProgram( program_ );
    }
    program( program &&rhs ): program_( 0 )
    {
        *this = std::move( rhs );
    }
    program &operator=( program &&rhs )
    {
        if( program_ )
            glDeleteProgram( program_ );

        program_ = rhs.program_;
        rhs.program_ = 0;
        shaders_ = std::move( rhs.shaders_ );
        uniform_locations_ = std::move( rhs.uniform_locations_ );
        return *this;
    }


private:
    std::vector<shader> shaders_;
    GLuint program_;
    std::vector<GLuint> uniform_locations_;

    template<typename... ArgsType>
    struct set_uniforms_from_impl_;

    template<typename FirstArgType, typename... ArgsType>
    struct set_uniforms_from_impl_<FirstArgType, ArgsType...>
    {
        static void run( program &p, GLuint index_from, FirstArgType &&first_arg, ArgsType &&... args )
        {
            p.set_uniform( index_from, std::forward<FirstArgType>( first_arg ) );
            set_uniforms_from_impl_<ArgsType...>::run( p, index_from + 1, std::forward<ArgsType>( args )... );
        }
    };

    template<>
    struct set_uniforms_from_impl_<>
    {
        static void run( program &p, GLuint index_from ) {}
    };
};

template<> inline void program::set_uniform<double>( GLuint index, const double &value )
{
    _gls_detail::check_bound( *this );
    glUniform1d( uniform_locations_[index], value );
}
template<> inline void program::set_uniform<float>( GLuint index, const float &value )
{
    _gls_detail::check_bound( *this );
    glUniform1f( uniform_locations_[index], value );
}
template<> inline void program::set_uniform<int>( GLuint index, const int &value )
{
    _gls_detail::check_bound( *this );
    glUniform1i( uniform_locations_[index], value );
}
template<> inline void program::set_uniform<unsigned int>( GLuint index, const unsigned int &value )
{
    _gls_detail::check_bound( *this );
    glUniform1ui( uniform_locations_[index], value );
}

template<> inline void program::set_uniform<glm::dvec2>( GLuint index, const glm::dvec2 &value )
{
    _gls_detail::check_bound( *this );
    glUniform2dv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::vec2>( GLuint index, const glm::vec2 &value )
{
    _gls_detail::check_bound( *this );
    glUniform2fv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::ivec2>( GLuint index, const glm::ivec2 &value )
{
    _gls_detail::check_bound( *this );
    glUniform2iv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::uvec2>( GLuint index, const glm::uvec2 &value )
{
    _gls_detail::check_bound( *this );
    glUniform2uiv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}

template<> inline void program::set_uniform<glm::dvec3>( GLuint index, const glm::dvec3 &value )
{
    _gls_detail::check_bound( *this );
    glUniform3dv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::vec3>( GLuint index, const glm::vec3 &value )
{
    _gls_detail::check_bound( *this );
    glUniform3fv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::ivec3>( GLuint index, const glm::ivec3 &value )
{
    _gls_detail::check_bound( *this );
    glUniform3iv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::uvec3>( GLuint index, const glm::uvec3 &value )
{
    _gls_detail::check_bound( *this );
    glUniform3uiv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}

template<> inline void program::set_uniform<glm::dvec4>( GLuint index, const glm::dvec4 &value )
{
    _gls_detail::check_bound( *this );
    glUniform4dv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::vec4>( GLuint index, const glm::vec4 &value )
{
    _gls_detail::check_bound( *this );
    glUniform4fv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::ivec4>( GLuint index, const glm::ivec4 &value )
{
    _gls_detail::check_bound( *this );
    glUniform4iv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::uvec4>( GLuint index, const glm::uvec4 &value )
{
    _gls_detail::check_bound( *this );
    glUniform4uiv( uniform_locations_[index], 1, glm::value_ptr( value ) );
}

template<> inline void program::set_uniform<glm::dmat2>( GLuint index, const glm::dmat2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat2>( GLuint index, const glm::mat2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat2x3>( GLuint index, const glm::dmat2x3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2x3dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat2x3>( GLuint index, const glm::mat2x3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2x3fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat2x4>( GLuint index, const glm::dmat2x4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2x4dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat2x4>( GLuint index, const glm::mat2x4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix2x4fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}

template<> inline void program::set_uniform<glm::dmat3>( GLuint index, const glm::dmat3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat3>( GLuint index, const glm::mat3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat3x2>( GLuint index, const glm::dmat3x2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3x2dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat3x2>( GLuint index, const glm::mat3x2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3x2fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat3x4>( GLuint index, const glm::dmat3x4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3x4dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat3x4>( GLuint index, const glm::mat3x4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix3x4fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}

template<> inline void program::set_uniform<glm::dmat4>( GLuint index, const glm::dmat4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat4>( GLuint index, const glm::mat4 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat4x2>( GLuint index, const glm::dmat4x2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4x2dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat4x2>( GLuint index, const glm::mat4x2 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4x2fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::dmat4x3>( GLuint index, const glm::dmat4x3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4x3dv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}
template<> inline void program::set_uniform<glm::mat4x3>( GLuint index, const glm::mat4x3 &value )
{
    _gls_detail::check_bound( *this );
    glUniformMatrix4x3fv( uniform_locations_[index], 1, GL_FALSE, glm::value_ptr( value ) );
}

namespace _gls_detail
{
struct no_change_t {};
}
constexpr _gls_detail::no_change_t no_change{};

template<> inline void program::set_uniform<_gls_detail::no_change_t>( GLuint index, const _gls_detail::no_change_t &value ) {}

class buffer
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        switch( target_ )
        {
        case GL_ARRAY_BUFFER:
            return GL_ARRAY_BUFFER_BINDING;
        //case GL_ATOMIC_COUNTER_BUFFER:
        //case GL_COPY_READ_BUFFER:
        //case GL_COPY_WRITE_BUFFER:
        case GL_DISPATCH_INDIRECT_BUFFER:
            return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
        //case GL_DRAW_INDIRECT_BUFFER:
        case GL_ELEMENT_ARRAY_BUFFER:
            return GL_ELEMENT_ARRAY_BUFFER_BINDING;
        case GL_PIXEL_PACK_BUFFER:
            return GL_PIXEL_PACK_BUFFER_BINDING;
        case GL_PIXEL_UNPACK_BUFFER:
            return GL_PIXEL_UNPACK_BUFFER_BINDING;
        //case GL_QUERY_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
            return GL_SHADER_STORAGE_BUFFER_BINDING;
        //case GL_TEXTURE_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
        case GL_UNIFORM_BUFFER:
            return GL_UNIFORM_BUFFER_BINDING;
        default:
            return 0;
        }
    }
    GLuint get() const
    {
        return buffer_;
    }
    void bind()
    {
        glBindBuffer( target_, buffer_ );
    }
    void unbind()
    {
        glBindBuffer( target_, 0 );
    }

public:
    buffer() : buffer_( 0 ), num_elements_( 0 ), size_element_( 0 ) {}
    buffer( GLenum target, GLenum usage ): target_( target ), usage_( usage ), num_elements_( 0 ), size_element_( 0 )
    {
        glGenBuffers( 1, &buffer_ );
    }
    buffer( const buffer & ) = delete;
    buffer &operator=( const buffer & ) = delete;
    virtual ~buffer()
    {
        if( buffer_ )
            glDeleteBuffers( 1, &buffer_ );

    }
    buffer( buffer &&rhs )
    {
        *this = std::move( rhs );
    }
    buffer &operator=( buffer &&rhs )
    {
        if( buffer_ )
            glDeleteBuffers( 1, &buffer_ );

        buffer_ = rhs.buffer_;
        rhs.buffer_ = 0;
        target_ = rhs.target_;
        usage_ = rhs.usage_;
        num_elements_ = rhs.num_elements_;
        rhs.num_elements_ = 0;
        size_element_ = rhs.size_element_;
        rhs.size_element_ = 0;
        return *this;
    }

    std::size_t size() const
    {
        return num_elements_ * size_element_;
    }

    void set_data( const void *data, size_t num_elements, size_t size_element )
    {
        _gls_detail::check_bound( *this );
        num_elements_ = num_elements;
        size_element_ = size_element;
        glBufferData( target_, size(), data, usage_ );
    }

    template<typename T>
    void set_data( const std::vector<T> &data )
    {
        set_data( &data[0], data.size(), sizeof( T ) );
    }

    std::size_t num_elements() const
    {
        return num_elements_;
    }
    std::size_t size_element() const
    {
        return size_element_;
    }
    GLenum target() const
    {
        return target_;
    }
private:
    GLuint buffer_;
    GLenum target_;
    GLenum usage_;
    std::size_t num_elements_;
    std::size_t size_element_;
};

class vertex_array
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        return GL_VERTEX_ARRAY_BINDING;
    }
    GLuint get() const
    {
        return va_;
    }
    void bind()
    {
        glBindVertexArray( va_ );
    }
    void unbind()
    {
        glBindVertexArray( 0 );
    }
public:
    vertex_array()
    {
        glGenVertexArrays( 1, &va_ );
    }
    vertex_array( nullptr_t ): va_( 0 ) {}
    vertex_array( const vertex_array & ) = delete;
    vertex_array &operator=( const vertex_array & ) = delete;
    virtual ~vertex_array()
    {
        if( va_ )
            glDeleteVertexArrays( 1, &va_ );
    }
    vertex_array( vertex_array &&rhs )
    {
        *this = std::move( rhs );
    }
    vertex_array &operator=( vertex_array &&rhs )
    {
        if( va_ )
            glDeleteVertexArrays( 1, &va_ );

        va_ = rhs.va_;
        rhs.va_ = 0;
        return *this;
    }

    void set_attribute( GLuint index, buffer &buf, GLint element_dimension, GLenum element_type, bool normalize, GLsizei stride, std::uintptr_t offset )
    {
        _gls_detail::check_bound( *this );
        assert( buf.target() == GL_ARRAY_BUFFER );
        glEnableVertexAttribArray( index );

        buf.bind();
        glVertexAttribPointer( index, element_dimension, element_type, normalize ? GL_TRUE : GL_FALSE, stride, reinterpret_cast<const GLvoid *>( offset ) );
    }
    void clear_attribute( GLuint index )
    {
        _gls_detail::check_bound( *this );
        glDisableVertexAttribArray( index );
    }
private:
    GLuint va_;
};

template<typename ColorMapType, typename DepthMapType>
class framebuffer
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        return GL_FRAMEBUFFER_BINDING;
    }
    GLuint get() const
    {
        return framebuffer_;
    }
    void bind()
    {
        glBindFramebuffer( GL_FRAMEBUFFER, framebuffer_ );
    }
    void unbind()
    {
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }
public:
    framebuffer() : framebuffer_( 0 ), width_( 0 ), height_( 0 ) {}
    framebuffer( int width, int height, bool screen = false ): framebuffer_( 0 ), width_( width ), height_( height )
    {
        if( screen )
            return;
        glGenFramebuffers( 1, &framebuffer_ );

        color_map_ = std::move( ColorMapType::generate_rgb_buffer( width, height ) );
        depth_map_ = std::move( DepthMapType::generate_depth_buffer( width, height ) );

        bind();

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        color_map_->attach_on( *this, GL_COLOR_ATTACHMENT0 );
        depth_map_->attach_on( *this, GL_DEPTH_ATTACHMENT );

		assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
    }

    void set_viewport()
    {
        glViewport( 0, 0, width_, height_ );
    }

    framebuffer( const framebuffer & ) = delete;
    framebuffer &operator=( const framebuffer & ) = delete;
    virtual ~framebuffer()
    {
        if( framebuffer_ )
            glDeleteFramebuffers( 1, &framebuffer_ );
    }
    framebuffer( framebuffer &&rhs )
    {
        *this = std::move( rhs );
    }
    framebuffer &operator=( framebuffer &&rhs )
    {
        if( framebuffer_ )
            glDeleteFramebuffers( 1, &framebuffer_ );

        framebuffer_ = rhs.framebuffer_;
        rhs.framebuffer_ = 0;
        color_map_ = std::move( rhs.color_map_ );
        depth_map_ = std::move( rhs.depth_map_ );
        width_ = rhs.width_;
        height_ = rhs.height_;
        return *this;
    }
    ColorMapType &get_color_map()
    {
        return *color_map_.get();
    }
    DepthMapType &get_depth_map()
    {
        return *depth_map_.get();
    }
private:
    GLuint framebuffer_;
    std::unique_ptr<ColorMapType> color_map_;
    std::unique_ptr<DepthMapType> depth_map_;
    int width_, height_;
};


template<typename ColorMapType, typename DepthMapType>
class cubemap_framebuffer
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        return GL_FRAMEBUFFER_BINDING;
    }
    GLuint get( int i ) const
    {
        return framebuffer_;
    }
    void bind( int i )
    {
        glBindFramebuffer( GL_FRAMEBUFFER, framebuffer_[i] );
    }
    void unbind()
    {
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }
public:
    cubemap_framebuffer()
    {
        framebuffer_[0] = 0;
    }
    cubemap_framebuffer( int size ): size_( size )
    {
        glGenFramebuffers( 6, framebuffer_ );

        color_map_ = std::move( ColorMapType::generate_cube_rgb_buffer( size ) );
        depth_map_ = std::move( DepthMapType::generate_cube_depth_buffer( size ) );

        color_map_->attach_on( *this, GL_COLOR_ATTACHMENT0 );
        depth_map_->attach_on( *this, GL_DEPTH_ATTACHMENT );

        for( int i = 0; i < 6; ++i )
        {
            bind( i );
            glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

            assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
        }
    }

    cubemap_framebuffer( const cubemap_framebuffer & ) = delete;
    cubemap_framebuffer &operator=( const cubemap_framebuffer & ) = delete;
    virtual ~cubemap_framebuffer()
    {
        if( framebuffer_[0] )
            glDeleteFramebuffers( 6, framebuffer_ );
    }
    cubemap_framebuffer( cubemap_framebuffer &&rhs )
    {
        *this = std::move( rhs );
    }
    cubemap_framebuffer &operator=( cubemap_framebuffer &&rhs )
    {
        if( framebuffer_[0] )
            glDeleteFramebuffers( 6, framebuffer_ );

        for( int i = 0; i < 6; ++i )
        {
            framebuffer_[i] = rhs.framebuffer_[i];
            rhs.framebuffer_[i] = 0;
        }
        color_map_ = std::move( rhs.color_map_ );
        depth_map_ = std::move( rhs.depth_map_ );
        size_ = rhs.size_;
        return *this;
    }

    void set_viewport()
    {
        glViewport( 0, 0, size_, size_ );
    }
    ColorMapType &get_color_map()
    {
        return *color_map_.get();
    }
    DepthMapType &get_depth_map()
    {
        return *depth_map_.get();
    }
private:
    GLuint framebuffer_[6];
    std::unique_ptr<ColorMapType> color_map_;
    std::unique_ptr<DepthMapType> depth_map_;
    int size_;
};


class texture
{
public:
    //common interfaces
    GLenum get_bind_parameter_name() const
    {
        switch( target_ )
        {
        case GL_TEXTURE_2D:
            return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_CUBE_MAP:
            return GL_TEXTURE_BINDING_CUBE_MAP;
        default:
            return 0;
        }
    }
    GLuint get() const
    {
        return texture_;
    }
    void bind()
    {
        glBindTexture( target_, texture_ );
    }
    void unbind()
    {
        glBindTexture( target_, 0 );
    }
public:
    texture() : texture_( 0 ) {}
    texture( GLenum target ) : target_( target )
    {
        glGenTextures( 1, &texture_ );
    }
    texture( const texture & ) = delete;
    texture &operator=( const texture & ) = delete;
    virtual ~texture()
    {
        if( texture_ )
            glDeleteTextures( 1, &texture_ );
    }
    texture( texture &&rhs )
    {
        *this = std::move( rhs );
    }
    texture &operator=( texture &&rhs )
    {
        if( texture_ )
            glDeleteTextures( 1, &texture_ );

        texture_ = rhs.texture_;
        rhs.texture_ = 0;
        target_ = rhs.target_;
        return *this;
    }

    template<typename ColorMapType, typename DepthMapType>
    void attach_on( framebuffer<ColorMapType, DepthMapType> &fb, GLenum attachment_target = GL_COLOR_ATTACHMENT0, GLint mipmap_level = 0 )
    {
        _gls_detail::check_bound( fb );
        glFramebufferTexture( GL_FRAMEBUFFER, attachment_target, texture_, mipmap_level );

		if ( attachment_target == GL_COLOR_ATTACHMENT0 )
			glDrawBuffers( 1, &attachment_target );
    }

    template<typename ColorMapType, typename DepthMapType>
    void attach_on( cubemap_framebuffer<ColorMapType, DepthMapType> &fb, GLenum attachment_target = GL_COLOR_ATTACHMENT0, GLint mipmap_level = 0 )
    {
        for( int i = 0; i < 6; ++i )
        {
			fb.bind( i );

            glFramebufferTexture2D( GL_FRAMEBUFFER, attachment_target, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, texture_, mipmap_level );

			if ( attachment_target == GL_COLOR_ATTACHMENT0 )
				glDrawBuffers( 1, &attachment_target );
        }
    }

    inline static std::unique_ptr<texture> generate_rgb_buffer( int width, int height );
    inline static std::unique_ptr<texture> generate_depth_buffer( int width, int height );
    inline static std::unique_ptr<texture> generate_cube_rgb_buffer( int size );
    inline static std::unique_ptr<texture> generate_cube_depth_buffer( int size );
protected:
    GLenum target_;
    GLuint texture_;
};

inline std::unique_ptr<texture> texture::generate_rgb_buffer( int width, int height )
{
    std::unique_ptr<texture> tex( new texture( GL_TEXTURE_2D ) );

    tex->bind();
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );

    return tex;
}
inline std::unique_ptr<texture> texture::generate_depth_buffer( int width, int height )
{
    std::unique_ptr<texture> tex( new texture( GL_TEXTURE_2D ) );

    tex->bind();
    glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE );

    return tex;
}
inline std::unique_ptr<texture> texture::generate_cube_rgb_buffer( int size )
{
    std::unique_ptr<texture> tex( new texture( GL_TEXTURE_CUBE_MAP ) );
    tex->bind();

    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0 );

    return tex;
}

inline std::unique_ptr<texture> texture::generate_cube_depth_buffer( int size )
{
    std::unique_ptr<texture> tex( new texture( GL_TEXTURE_CUBE_MAP ) );
    tex->bind();

    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );

    return tex;
}

template<std::size_t NumTextures>
class gbuffer_texture
{
public:
	static constexpr std::size_t num_textures = NumTextures;

    //common interfaces
    GLenum get_bind_parameter_name() const
    {
		return GL_TEXTURE_BINDING_2D;
    }
    GLuint get( int idx ) const
    {
        return texture_[ idx ];
    }
    void bind( int idx )
    {
		glBindTexture( GL_TEXTURE_2D, texture_[ idx ] );
    }
    void unbind()
    {
		glBindTexture( GL_TEXTURE_2D, 0 );
    }
public:

    gbuffer_texture()
    {
		for ( int i = 0; i < num_textures; ++i )
		{
			texture_[ i ] = 0 ;
		}

        glGenTextures( num_textures, texture_ ) ;
    }
    gbuffer_texture( const gbuffer_texture & ) = delete;
    gbuffer_texture &operator=( const gbuffer_texture & ) = delete;
    virtual ~gbuffer_texture()
    {
        if( texture_[ 0 ] )
            glDeleteTextures( num_textures, texture_ );
    }
    gbuffer_texture( gbuffer_texture &&rhs )
    {
        *this = std::move( rhs );
    }
    gbuffer_texture &operator=( gbuffer_texture &&rhs )
    {
        if( texture_[ 0 ] )
            glDeleteTextures( num_textures, texture_ );

		for ( int i = 0; i < num_textures; ++i )
		{
			texture_[ i ] = rhs.texture_[ i ];
			rhs.texture_[ i ] = 0;
		}
        return *this;
    }

	template<typename ColorMapType, typename DepthMapType>
    void attach_on( framebuffer<ColorMapType, DepthMapType> &fb, GLenum attachment_target = GL_COLOR_ATTACHMENT0, GLint mipmap_level = 0 )
    {
        _gls_detail::check_bound( fb );
		assert(attachment_target == GL_COLOR_ATTACHMENT0);

		GLenum buffers[num_textures];

		for(GLenum i = 0; i < num_textures; ++i) {
			buffers[i] = i + GL_COLOR_ATTACHMENT0;
			glFramebufferTexture( GL_FRAMEBUFFER, buffers[i], texture_[i], mipmap_level );
		}

		glDrawBuffers( num_textures, buffers );
    }

    template<typename ColorMapType, typename DepthMapType>
    void attach_on( cubemap_framebuffer<ColorMapType, DepthMapType> &fb, GLenum attachment_target = GL_COLOR_ATTACHMENT0, GLint mipmap_level = 0 )
    {
		assert(attachment_target == GL_COLOR_ATTACHMENT0);

		GLenum buffers[num_textures];
		for(GLenum i = 0; i < num_textures; ++i)
			buffers[i] = i + GL_COLOR_ATTACHMENT0;

		for( int i = 0; i < 6; ++i )
		{
			fb.bind( i );

			for(GLenum j = 0; j < num_textures; ++j)
				glFramebufferTexture2D( GL_FRAMEBUFFER, buffers[j], GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, texture_[j], mipmap_level );

			glDrawBuffers( num_textures, buffers );
        }
    }

    inline static std::unique_ptr<gbuffer_texture> generate_rgb_buffer( int width, int height );

protected:

    GLuint texture_[ num_textures ];
};

template<std::size_t NumTextures>
inline std::unique_ptr<gbuffer_texture<NumTextures> > gbuffer_texture<NumTextures>::generate_rgb_buffer( int width, int height )
{
    std::unique_ptr<gbuffer_texture> tex( new gbuffer_texture() );

	for ( int i = 0; i < num_textures; ++i )
	{
		tex->bind( i ) ;
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGB, GL_FLOAT, 0 ) ;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) ;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) ;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER ) ;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER ) ;
	}

    return tex ;
}

#define GLS_CHECK_ERROR()\
	{\
		GLenum err = glGetError();\
		if (err != GL_NO_ERROR)\
		{\
			for (; err != GL_NO_ERROR; err = glGetError())\
			{\
				switch (err)\
				{\
					case GL_INVALID_ENUM: printf("GL_INVALID_ENUM, "); break;\
					case GL_INVALID_OPERATION: printf("GL_INVALID_OPERATION, "); break;\
					case GL_INVALID_VALUE: printf("GL_INVALID_VALUE, "); break;\
					case GL_OUT_OF_MEMORY: printf("GL_OUT_OF_MEMORY, "); break;\
					default: printf("OpenGL Error Code: %d, ", err);\
				}\
			}\
		}\
	}

}

#endif
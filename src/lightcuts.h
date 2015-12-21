#ifndef _LIGHTCUTS_H_
#define _LIGHTCUTS_H_

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <vector>
#include "raytracer.h"


class light_node_t
{
public :
    
	light_node_t()
		: m_bLeafNode( false ), m_iTreeNodeIndex( -1 ), m_iParentNodeIndex( -1 ), m_iLeftChildNodeIndex( -1 ), m_iRightChildNodeIndex( -1 )
		, m_iVplIndex( -1 )
	{
		m_vPosition = glm::vec3( 0, 0, 0 ) ;
		m_vIntensity = glm::vec3( 0, 0, 0 ) ;
		m_vMinAABB = glm::vec3( 0, 0, 0 ) ;
		m_vMaxAABB = glm::vec3( 0, 0, 0 ) ;
		m_vDirection = glm::vec3( 0, 0, 0 ) ;

		m_fIntensity = 0.0f ;

		m_fError = 0.0f ;
	}

	bool			m_bLeafNode ;
	
	int				m_iTreeNodeIndex ;
	int				m_iParentNodeIndex ;
	int				m_iLeftChildNodeIndex ;
	int				m_iRightChildNodeIndex ;

	int				m_iVplIndex ;
	glm::vec3		m_vPosition ;
	glm::vec3		m_vIntensity ;
	glm::vec3		m_vMinAABB ;
    glm::vec3		m_vMaxAABB ;
	glm::vec3		m_vDirection ;
	
	float			m_fIntensity ;
	
	float			m_fError ;
	
protected :
	
} ;

class cube_depth_map_t
{
public :

	// constructor
	cube_depth_map_t( int shadowSize ) ;
	virtual ~cube_depth_map_t() ;

	void	setCubeDepthMap( GLenum cube_map_pos, float* pTex, std::size_t tex_size ) ;

	float	getDepth( glm::vec3 vDir ) const ;

public :

	int						m_iShadowSize ;
	//std::vector< float >	m_arrCubeDepthMap[ 6 ] ;
	float*					m_arrCubeDepthMap[ 6 ] ;

} ;


class lightcuts_t
{
	static constexpr float	kBoundError = 0.02f ;

	static constexpr int	kWSampleSize = 5 ;
	static constexpr int	kHSampleSize = 5 ;

public :

	lightcuts_t() ;
	virtual ~lightcuts_t() ;

	bool	build_light_tree( const std::vector< point_light_t >& vpls ) ;

	bool	calculate_global_lightcut( const float* pos, const float* normal, int w, int h, int iVplSize ) ;

	bool	calculate_global_lightcut_by_max_local_lightcut( const float* pos, const float* normal, int w, int h, int iVplSize ) ;
	bool	calculate_global_lightcut_by_used_node_count( const float* pos, const float* normal, int w, int h, int iVplSize ) ;
	bool	calculate_global_lightcut_by_used_node_count_with_error( const float* pos, const float* normal, int w, int h, int iVplSize ) ;

protected :

	// for light tree
	int		get_tree_node_count( int iLeafCount ) ;

	int		get_best_match_light_index( int iStartIndex, int iEndIndex, int iCurrIndex, std::vector< bool >& vLeafTaken ) ;

	float	similarity_metric( const light_node_t& light1, const light_node_t& light2 ) ;

	int		choose_representative_light( int iIndex1, int iIndex2 ) ;

	// for light cut
	glm::vec3	calculate_L( const light_node_t& light, const glm::vec3& p, const glm::vec3& n ) ;

	std::vector< light_node_t >	calculate_local_lightcut( const glm::vec3& p, const glm::vec3 n, const int kMaxLightsInCut ) ;

	// test
	void	make_light_cut( const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, int kMaxLightsInCut = 0 ) ;

	int		get_child_node_used_count( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, float& fOutError ) ;
	int		get_child_node_max_used_count( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, float& fOutMaxError ) ;

	void	add_valid_node( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, int kMaxLightsInCut, int iTreeLevel ) ;

public :

	std::vector< light_node_t >			m_vLightTree ;
	light_node_t						m_root ;

	std::vector< light_node_t >			m_vLightCut ;

	//std::vector< cube_depth_map_t >		cube_depth_map ;
	std::vector< cube_depth_map_t* >		cube_depth_map ;

} ;

#endif

#include "lightcuts.h"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <GLFW/glfw3.h>
#include <iostream>


lightcuts_t::lightcuts_t()
{
	m_vLightTree.clear() ;
	m_vLightCut.clear() ;
}

lightcuts_t::~lightcuts_t()
{
	int iSize = cube_depth_map.size() ;
	for ( int i = 0; i < iSize; ++i )
	{
		if ( cube_depth_map[ i ] )
		{
			delete cube_depth_map[ i ] ;
			cube_depth_map[ i ] = NULL ;
		}
	}
}

bool	lightcuts_t::build_light_tree( const std::vector<point_light_t>& vpls )//_origin )
{
	//std::vector< point_light_t > vpls ;
	//vpls.assign( vpls_origin.begin(), vpls_origin.begin() + 11 ) ;

	double timeStart = glfwGetTime() ;

	m_vLightTree.clear() ;

	int iVplCount = (int)vpls.size() ;
	int iTreeNodeCount = get_tree_node_count( iVplCount ) ;
	
	light_node_t emptyNode ;
	m_vLightTree.reserve( iTreeNodeCount ) ;
	m_vLightTree.assign( iTreeNodeCount, emptyNode ) ;

	std::vector< bool > vLeafTaken ;
	vLeafTaken.reserve( iTreeNodeCount ) ;
	vLeafTaken.assign( iTreeNodeCount, false ) ;

	// fill leaf nodes
	std::cout << "fill leaf nodes ... " << iVplCount << std::endl ;

	for ( int i = 0; i < iVplCount; ++i )
	{
		m_vLightTree[ i ].m_bLeafNode = true ;
		
		m_vLightTree[ i ].m_iTreeNodeIndex = i ;
		m_vLightTree[ i ].m_iParentNodeIndex = -1 ;
		m_vLightTree[ i ].m_iLeftChildNodeIndex = -1 ;
		m_vLightTree[ i ].m_iRightChildNodeIndex = -1 ;
		
		m_vLightTree[ i ].m_iVplIndex = i ;
		m_vLightTree[ i ].m_vPosition = vpls[ i ].position ;
		m_vLightTree[ i ].m_vIntensity = vpls[ i ].intensity ;
		m_vLightTree[ i ].m_vMinAABB = vpls[ i ].position ;
		m_vLightTree[ i ].m_vMaxAABB = vpls[ i ].position ;
		m_vLightTree[ i ].m_vDirection = vpls[ i ].direction ;

		// TODO: assume that light has same r,g,b intensity
		m_vLightTree[ i ].m_fIntensity = std::max( std::max( m_vLightTree[ i ].m_vIntensity.r, m_vLightTree[ i ].m_vIntensity.g ), m_vLightTree[ i ].m_vIntensity.b ) ;
	}

	// build up cluster nodes
	std::cout << "build up cluster nodes ... " << std::endl ;
	int iLevel = 0 ;

	int iTreeNodeIndex = iVplCount ;

	int iCurrLayerNodeCount = iVplCount ;
	int iNodeStartIndex = 0 ;
	int iNodeEndIndex = iCurrLayerNodeCount ;

	bool bCurrLayerFinished = false ;
	bool bOneNodeRemained = false ; ;

	while ( iCurrLayerNodeCount > 1 )
	{
		// get 1st untaken light
		int iCurrIndex = -1 ;
		for ( int i = iNodeStartIndex; i < iNodeEndIndex; ++i )
		{
			if ( false == vLeafTaken[ i ] )
			{
				iCurrIndex = i ;
				break ;
			}
		}

		if ( -1 == iCurrIndex )
		{
			bCurrLayerFinished = true ;
			
			//std::cout << "failed to find node that is not taken !!" << std::endl ;
			//break ;
		}
		else
		{
			// find the best match light
			int iMatchIndex = get_best_match_light_index( iNodeStartIndex, iNodeEndIndex, iCurrIndex, vLeafTaken ) ;
			if ( -1 != iMatchIndex )
			{
				// found
				m_vLightTree[ iTreeNodeIndex ].m_bLeafNode = false ;
		
				m_vLightTree[ iTreeNodeIndex ].m_iTreeNodeIndex = iTreeNodeIndex ;
				m_vLightTree[ iTreeNodeIndex ].m_iParentNodeIndex = -1 ;
				m_vLightTree[ iTreeNodeIndex ].m_iLeftChildNodeIndex = iCurrIndex ;
				m_vLightTree[ iTreeNodeIndex ].m_iRightChildNodeIndex = iMatchIndex ;

				m_vLightTree[ iCurrIndex ].m_iParentNodeIndex = iTreeNodeIndex ;
				m_vLightTree[ iMatchIndex ].m_iParentNodeIndex = iTreeNodeIndex ;
		
				glm::vec3 vIntensity = m_vLightTree[ iCurrIndex ].m_vIntensity + m_vLightTree[ iMatchIndex ].m_vIntensity ;

				glm::vec3 vMin, vMax ;
				vMin.x = std::min( m_vLightTree[ iCurrIndex ].m_vMinAABB.x, m_vLightTree[ iMatchIndex ].m_vMinAABB.x ) ;
				vMin.y = std::min( m_vLightTree[ iCurrIndex ].m_vMinAABB.y, m_vLightTree[ iMatchIndex ].m_vMinAABB.y ) ;
				vMin.z = std::min( m_vLightTree[ iCurrIndex ].m_vMinAABB.z, m_vLightTree[ iMatchIndex ].m_vMinAABB.z ) ;

				vMax.x = std::max( m_vLightTree[ iCurrIndex ].m_vMaxAABB.x, m_vLightTree[ iMatchIndex ].m_vMaxAABB.x ) ;
				vMax.y = std::max( m_vLightTree[ iCurrIndex ].m_vMaxAABB.y, m_vLightTree[ iMatchIndex ].m_vMaxAABB.y ) ;
				vMax.z = std::max( m_vLightTree[ iCurrIndex ].m_vMaxAABB.z, m_vLightTree[ iMatchIndex ].m_vMaxAABB.z ) ;

				glm::vec3 vDirection = m_vLightTree[ iCurrIndex ].m_vDirection + m_vLightTree[ iMatchIndex ].m_vDirection ;
				if ( 0 == vDirection.x && 0 == vDirection.y && 0 == vDirection.z )
				{
					// TODO: it is a protection code for zero direction problem
					vDirection = m_vLightTree[ iCurrIndex ].m_vDirection ;
				}
				//vDirection = glm::normalize( vDirection ) ;

				int iRepresentativeVplIndex = choose_representative_light( iCurrIndex, iMatchIndex ) ;
				
				m_vLightTree[ iTreeNodeIndex ].m_iVplIndex = iRepresentativeVplIndex ;
				m_vLightTree[ iTreeNodeIndex ].m_vPosition = vpls[ iRepresentativeVplIndex ].position ;
				m_vLightTree[ iTreeNodeIndex ].m_vIntensity = vIntensity ;
				m_vLightTree[ iTreeNodeIndex ].m_vMinAABB = vMin ;
				m_vLightTree[ iTreeNodeIndex ].m_vMaxAABB = vMax ;
				m_vLightTree[ iTreeNodeIndex ].m_vDirection = vDirection ;

				// TODO: assume that light has same r,g,b intensity
				m_vLightTree[ iTreeNodeIndex ].m_fIntensity = std::max( std::max( vIntensity.r, vIntensity.g ), vIntensity.b ) ;

				vLeafTaken[ iCurrIndex ] = true ;
				vLeafTaken[ iMatchIndex ] = true ;

				++iTreeNodeIndex ;
			}
			else
			{
				// it is the last one
				// move it to the upper layer (swap last node)

				if ( iCurrIndex != iNodeEndIndex - 1 )
				{
					light_node_t temp = m_vLightTree[ iCurrIndex ] ;
					m_vLightTree[ iCurrIndex ] = m_vLightTree[ iNodeEndIndex - 1 ] ;
					m_vLightTree[ iNodeEndIndex - 1 ] = temp ;

					m_vLightTree[ iCurrIndex ].m_iTreeNodeIndex = iCurrIndex ;
					m_vLightTree[ iNodeEndIndex - 1 ].m_iTreeNodeIndex = iNodeEndIndex - 1 ;

					vLeafTaken[ iNodeEndIndex - 1 ] = false ;
					vLeafTaken[ iCurrIndex ] = true ;

					for ( int i = iNodeEndIndex; i < iTreeNodeIndex; ++i )
					{
						if ( iNodeEndIndex - 1 == m_vLightTree[ i ].m_iLeftChildNodeIndex )
						{
							m_vLightTree[ i ].m_iLeftChildNodeIndex = iCurrIndex ;
							break ;
						}

						if ( iNodeEndIndex - 1 == m_vLightTree[ i ].m_iRightChildNodeIndex )
						{
							m_vLightTree[ i ].m_iRightChildNodeIndex = iCurrIndex ;
							break ;
						}
					}
				}

				bCurrLayerFinished = true ;
				bOneNodeRemained = true ;
			}
		}

		if ( bCurrLayerFinished )
		{
			int iNodeCount = iCurrLayerNodeCount ;
			
			iNodeStartIndex = iNodeEndIndex ;

			iCurrLayerNodeCount = iCurrLayerNodeCount / 2 ;
			if ( bOneNodeRemained )
			{
				++iCurrLayerNodeCount ;
				
				--iNodeStartIndex ;

				std::cout << "[level " << iLevel << "] - " << iNodeCount - 1 << " nodes (move one node to next level) ..." << std::endl ;
			}
			else
			{
				if ( iCurrLayerNodeCount > 1 )
				{
					std::cout << "[level " << iLevel << "] - " << iNodeCount << " nodes ..." << std::endl ;
				}
				else
				{
					std::cout << "[level " << iLevel << "] - root node ..." << std::endl ;

					m_root = m_vLightTree[ iTreeNodeIndex - 1 ] ;
				}
			}

			iNodeEndIndex = iTreeNodeIndex ;

			bCurrLayerFinished = false ;
			bOneNodeRemained = false ;

			++iLevel ;
		}
	}

	double timeEnd = glfwGetTime() ;
	std::cout << "time : " << timeEnd - timeStart << std::endl ;

	std::cout << "success to build light tree !!" << std::endl ;

	return true ;
}

int		lightcuts_t::get_tree_node_count( int iLeafCount )
{
	int iNodeCount = 0 ;
	int iRemain = 0 ;

	if ( iLeafCount > 1 )
	{
		while ( iLeafCount + iRemain >= 2 )
		{
			iLeafCount += iRemain ;

			iRemain = iLeafCount % 2 ;
			if ( 1 == iRemain )
			{
				// move remain node to upper layer
				--iLeafCount ;
			}

			iNodeCount += iLeafCount ;
			iLeafCount /= 2 ;
		}

		++iNodeCount ;	// root node
	}

	return iNodeCount ;
}

int		lightcuts_t::get_best_match_light_index( int iStartIndex, int iEndIndex, int iCurrIndex, std::vector< bool >& vLeafTaken )
{
	int iIndex = -1 ;

	float fCurrentMetric = -1.0f ;

	for ( int i = iStartIndex; i < iEndIndex; ++i )
	{
		if ( i != iCurrIndex && false == vLeafTaken[ i ] )
		{
			float fMetric = similarity_metric( m_vLightTree[ iCurrIndex ], m_vLightTree[ i ] ) ;
			if ( -1.0f == fCurrentMetric || fMetric < fCurrentMetric )
			{
				fCurrentMetric = fMetric ;
				iIndex = i ;
			}
		}
	}

	return iIndex ;
}

float	lightcuts_t::similarity_metric( const light_node_t& light1, const light_node_t& light2 )
{
	glm::vec3 vIntensity = light1.m_vIntensity - light2.m_vIntensity ;
	float fIntensity = glm::length( vIntensity ) ;
	if ( 0 == fIntensity )
	{
		fIntensity = 1 ;
	}
	
	glm::vec3 vMinAABB = glm::vec3( 0 ) ;
	glm::vec3 vMaxAABB = glm::vec3( 0 ) ;

	vMinAABB.x = std::min( light1.m_vMinAABB.x, light2.m_vMinAABB.x ) ;
	vMinAABB.y = std::min( light1.m_vMinAABB.y, light2.m_vMinAABB.y ) ;
	vMinAABB.z = std::min( light1.m_vMinAABB.z, light2.m_vMinAABB.z ) ;

	vMaxAABB.x = std::max( light1.m_vMaxAABB.x, light2.m_vMaxAABB.x ) ;
	vMaxAABB.y = std::max( light1.m_vMaxAABB.y, light2.m_vMaxAABB.y ) ;
	vMaxAABB.z = std::max( light1.m_vMaxAABB.z, light2.m_vMaxAABB.z ) ;

	glm::vec3 vDigonal = vMaxAABB - vMinAABB ;
	float fDigonalLength = glm::length( vDigonal ) ;

	// TODO : replace fConst to diagonal length of scene bounding box
	float fConst = 1.0f ;

	// V1 dot V2 = |V1||V2|cos( Angle ) => half angle = acos( V1 dot V2 / |V1||V2| ) / 2
	float fHalfAngle = acosf( glm::dot( light1.m_vDirection, light2.m_vDirection ) / ( glm::length( light1.m_vDirection ) * glm::length( light2.m_vDirection ) ) ) / 2.0f ;

	return fIntensity * ( fDigonalLength * fDigonalLength + fConst * fConst * ( 1 - cosf( fHalfAngle ) ) * ( 1 - cosf( fHalfAngle ) ) ) ;
}

int		lightcuts_t::choose_representative_light( int iIndex1, int iIndex2 )
{
	const light_node_t& light1 = m_vLightTree[ iIndex1 ] ;
	const light_node_t& light2 = m_vLightTree[ iIndex2 ] ;

	float fIntensity1 = glm::length( light1.m_vIntensity ) ;
	float fIntensity2 = glm::length( light2.m_vIntensity ) ;

	float fProb1 = fIntensity1 / ( fIntensity1 + fIntensity2 ) ;
	float fRandom = rand() % 100 / 100.0f ;

	return ( fRandom < fProb1 ) ? m_vLightTree[ iIndex1 ].m_iVplIndex : m_vLightTree[ iIndex2 ].m_iVplIndex ;
}

bool	lightcuts_t::calculate_global_lightcut( const float* pos, const float* normal, int w, int h, int iVplSize )
{
	const int kMaxLightsInCut = (int)( iVplSize * 70.0f / 100.0f + 0.5f ) ;
	
	m_vLightCut.clear() ;

	if ( pos && normal )
	{
		int iWStep = w / kWSampleSize ;
		int iHStep = h / kHSampleSize ;

		int iSampleSize = kWSampleSize * kHSampleSize ;

		for ( int y = iWStep / 2; y < h; y += iWStep )
		{
			for ( int x = iHStep / 2; x < w; x += iHStep )
			{
				glm::vec3 p = glm::vec3( *( pos + y * w * 3 + x ), *( pos + y * w * 3 + x + 1 ), *( pos + y * w * 3 + x + 2 ) ) ;
				glm::vec3 n = glm::vec3( *( normal + y * w * 3 + x ), *( normal + y * w * 3 + x + 1 ), *( normal + y * w * 3 + x + 2 ) ) ;

				if ( glm::vec3( 0 ) == p && glm::vec3( 0 ) == n ) 
				{
					continue ;
				}

				const std::vector< light_node_t > vLocalLightCut = calculate_local_lightcut( p, n, kMaxLightsInCut ) ;
			}
		}
		
		// make light cut from leaf
		//make_light_cut( vUsedTreeNodeCount ) ;

		return true ;
	}

	return false ;
}

bool	lightcuts_t::calculate_global_lightcut_by_max_local_lightcut( const float* pos, const float* normal, int w, int h, int iVplSize )
{
	const int kMaxLightsInCut = (int)( iVplSize * 70.0f / 100.0f + 0.5f ) ;

	m_vLightCut.clear() ;


	// for statistics
	double timeStart = glfwGetTime() ;
	int iValidLocalLightcutCount = 0 ;
	double avgTime = 0 ;
	
	if ( pos && normal )
	{
		int iWStep = w / kWSampleSize ;
		int iHStep = h / kHSampleSize ;

		int iSampleSize = kWSampleSize * kHSampleSize ;

		for ( int y = iWStep / 2; y < h; y += iWStep )
		{
			for ( int x = iHStep / 2; x < w; x += iHStep )
			{
				glm::vec3 p = glm::vec3( *( pos + y * w * 3 + x ), *( pos + y * w * 3 + x + 1 ), *( pos + y * w * 3 + x + 2 ) ) ;
				glm::vec3 n = glm::vec3( *( normal + y * w * 3 + x ), *( normal + y * w * 3 + x + 1 ), *( normal + y * w * 3 + x + 2 ) ) ;

				if ( glm::vec3( 0 ) == p && glm::vec3( 0 ) == n ) 
				{
					continue ;
				}

				// for statistics
				++iValidLocalLightcutCount ;
				double timeLightcutStart = glfwGetTime() ;
				
				const std::vector< light_node_t > vLocalLightCut = calculate_local_lightcut( p, n, kMaxLightsInCut ) ;
				
				// for statistics
				double timeLightcutEnd = glfwGetTime() ;
				avgTime += timeLightcutEnd - timeLightcutStart ;

//				static int iLocalLightCutIndex = 0;
//				std::cout << iLocalLightCutIndex++ << " - cut size : " << vLocalLightCut.size() << std::endl ;

				if ( vLocalLightCut.size() > m_vLightCut.size() )
				{
					m_vLightCut.clear() ;

					m_vLightCut.assign( vLocalLightCut.begin(), vLocalLightCut.end() ) ;
				}
			}
		}
		
//		std::cout << "global light cut size : " << m_vLightCut.size() << std::endl ;

		// for statistics
		double timeEnd = glfwGetTime() ;
		std::cout << "local lightcut count : " << iValidLocalLightcutCount << ", avg time to make local lightcut : " << avgTime / iValidLocalLightcutCount << std::endl ;
		std::cout << "time to generate global lightcut : " << timeEnd - timeStart << std::endl ;

		return true ;
	}

	return false ;
}

bool	lightcuts_t::calculate_global_lightcut_by_used_node_count( const float* pos, const float* normal, int w, int h, int iVplSize )
{
	const int kMaxLightsInCut = (int)( iVplSize * 70.0f / 100.0f + 0.5f ) ;

	m_vLightCut.clear() ;

	
	// for statistics
	double timeStart = glfwGetTime() ;
	int iValidLocalLightcutCount = 0 ;
	double avgTime = 0 ;


	if ( pos && normal )
	{
		int iWStep = w / kWSampleSize ;
		int iHStep = h / kHSampleSize ;

		int iSampleSize = kWSampleSize * kHSampleSize ;

		int iTreeNodeSize = (int)m_vLightTree.size() ;

		std::vector< int > vUsedTreeNodeCount ;
		vUsedTreeNodeCount.reserve( iTreeNodeSize ) ;
		vUsedTreeNodeCount.assign( iTreeNodeSize, 0 ) ;

		std::vector< float > vEmptyError ;
		vEmptyError.clear() ;

		for ( int y = iWStep / 2; y < h; y += iWStep )
		{
			for ( int x = iHStep / 2; x < w; x += iHStep )
			{
				glm::vec3 p = glm::vec3( *( pos + y * w * 3 + x ), *( pos + y * w * 3 + x + 1 ), *( pos + y * w * 3 + x + 2 ) ) ;
				glm::vec3 n = glm::vec3( *( normal + y * w * 3 + x ), *( normal + y * w * 3 + x + 1 ), *( normal + y * w * 3 + x + 2 ) ) ;

				if ( glm::vec3( 0 ) == p && glm::vec3( 0 ) == n ) 
				{
					continue ;
				}

				// for statistics
				++iValidLocalLightcutCount ;
				double timeLightcutStart = glfwGetTime() ;

				const std::vector< light_node_t > vLocalLightCut = calculate_local_lightcut( p, n, kMaxLightsInCut ) ;

				// for statistics
				double timeLightcutEnd = glfwGetTime() ;
				avgTime += timeLightcutEnd - timeLightcutStart ;
				
//				static int iLocalLightCutIndex = 0;
//				std::cout << iLocalLightCutIndex++ << " - cut size : " << vLocalLightCut.size() << std::endl ;

				int iCutSize = (int)vLocalLightCut.size() ;
				for ( int i = 0; i < iCutSize; ++i )
				{
					++vUsedTreeNodeCount[ vLocalLightCut[ i ].m_iTreeNodeIndex ] ;
				}
			}
		}

		make_light_cut( vUsedTreeNodeCount, vEmptyError, kMaxLightsInCut ) ;

//		std::cout << "global light cut size : " << m_vLightCut.size() << std::endl ;

		// for statistics
		double timeEnd = glfwGetTime() ;
		std::cout << "local lightcut count : " << iValidLocalLightcutCount << ", avg time to make local lightcut : " << avgTime / iValidLocalLightcutCount << std::endl ;
		std::cout << "time to generate global lightcut : " << timeEnd - timeStart << std::endl ;

		return true ;
	}

	return false ;
}

bool	lightcuts_t::calculate_global_lightcut_by_used_node_count_with_error( const float* pos, const float* normal, int w, int h, int iVplSize )
{
	const int kMaxLightsInCut = (int)( iVplSize * 70.0f / 100.0f + 0.5f ) ;

	m_vLightCut.clear() ;


	// for statistics
	double timeStart = glfwGetTime() ;
	int iValidLocalLightcutCount = 0 ;
	double avgTime = 0 ;


	if ( pos && normal )
	{
		int iWStep = w / kWSampleSize ;
		int iHStep = h / kHSampleSize ;

		int iSampleSize = kWSampleSize * kHSampleSize ;

		int iTreeNodeSize = (int)m_vLightTree.size() ;

		std::vector< int > vUsedTreeNodeCount ;
		vUsedTreeNodeCount.reserve( iTreeNodeSize ) ;
		vUsedTreeNodeCount.assign( iTreeNodeSize, 0 ) ;

		std::vector< float > vUsedTreeNodeError ;
		vUsedTreeNodeError.reserve( iTreeNodeSize ) ;
		vUsedTreeNodeError.assign( iTreeNodeSize, 0.0f ) ;

		for ( int y = iWStep / 2; y < h; y += iWStep )
		{
			for ( int x = iHStep / 2; x < w; x += iHStep )
			{
				glm::vec3 p = glm::vec3( *( pos + y * w * 3 + x ), *( pos + y * w * 3 + x + 1 ), *( pos + y * w * 3 + x + 2 ) ) ;
				glm::vec3 n = glm::vec3( *( normal + y * w * 3 + x ), *( normal + y * w * 3 + x + 1 ), *( normal + y * w * 3 + x + 2 ) ) ;

				if ( glm::vec3( 0 ) == p && glm::vec3( 0 ) == n ) 
				{
					continue ;
				}

				// for statistics
				++iValidLocalLightcutCount ;
				double timeLightcutStart = glfwGetTime() ;

				const std::vector< light_node_t > vLocalLightCut = calculate_local_lightcut( p, n, kMaxLightsInCut ) ;

				// for statistics
				double timeLightcutEnd = glfwGetTime() ;
				avgTime += timeLightcutEnd - timeLightcutStart ;
				
//				static int iLocalLightCutIndex = 0;
//				std::cout << iLocalLightCutIndex++ << " - cut size : " << vLocalLightCut.size() << std::endl ;

				int iCutSize = (int)vLocalLightCut.size() ;
				for ( int i = 0; i < iCutSize; ++i )
				{
					++vUsedTreeNodeCount[ vLocalLightCut[ i ].m_iTreeNodeIndex ] ;
					vUsedTreeNodeError[ vLocalLightCut[ i ].m_iTreeNodeIndex ] += vLocalLightCut[ i ].m_fError ;
				}
			}
		}

		// normalize
		for (int i = 0; i < iTreeNodeSize; ++i )
		{
			if ( vUsedTreeNodeCount[ i ] > 0 )
			{
				vUsedTreeNodeError[ i ] /= vUsedTreeNodeCount[ i ] ;
			}
		}

		make_light_cut( vUsedTreeNodeCount, vUsedTreeNodeError, kMaxLightsInCut ) ;

//		std::cout << "global light cut size : " << m_vLightCut.size() << std::endl ;

		// for statistics
		double timeEnd = glfwGetTime() ;
		std::cout << "local lightcut count : " << iValidLocalLightcutCount << ", avg time to make local lightcut : " << avgTime / iValidLocalLightcutCount << std::endl ;
		std::cout << "time to generate global lightcut : " << timeEnd - timeStart << std::endl ;

		return true ;
	}

	return false ;
}


glm::vec3	lightcuts_t::calculate_L( const light_node_t& light, const glm::vec3& p, const glm::vec3& n )
{
	const float fBias = 0.05f ;

	int iVplIndex = light.m_iVplIndex ;

	glm::vec3 vPtoL = light.m_vPosition - p ;
	glm::vec3 vNormalizedPtoL = glm::normalize( vPtoL ) ;
	glm::vec3 vNormalizedVplDir = glm::normalize( light.m_vDirection ) ;

	float fLength = glm::length( vPtoL ) ;
	//float fShadowDepth = cube_depth_map[ iVplIndex ].getDepth( vNormalizedPtoL ) ;
	float fShadowDepth = cube_depth_map[ iVplIndex ]->getDepth( vNormalizedPtoL ) ;
				
	float V = ( fLength - fBias > fShadowDepth ) ? 0.0f : 1.0f ;
	float M = std::max( 0.0f, glm::dot( vNormalizedPtoL, n ) ) ;

	if ( 0 == fLength )
	{
		fLength = 0.0001f ;
	}
	float G = std::max( 0.0f, glm::dot( vNormalizedVplDir, -vNormalizedPtoL ) ) / fLength * fLength ;

	// TODO : zero value problem
	if ( 0 == V ) V = 0.0001f ;
	if ( 0 == M ) M = 0.0001f ;
	if ( 0 == G ) G = 0.0001f ;

	//return light.m_vIntensity * M * G * V ;
	return glm::vec3( light.m_fIntensity * M * G * V, light.m_fIntensity * M * G * V, light.m_fIntensity * M * G * V ) ;
}


std::vector< light_node_t >		lightcuts_t::calculate_local_lightcut( const glm::vec3& p, const glm::vec3 n, const int kMaxLightsInCut )
{
	std::vector< light_node_t > vLightcut ;

	glm::vec3 L = glm::vec3( 0 ) ;
	glm::vec3 estimatedL = glm::vec3( 0 ) ;
	glm::vec3 vError ;

	std::vector< int > vTreeNodeIndex ;
	std::vector< glm::vec3 > vTreeNodeErrors ;
	std::vector< bool > vTreeNodeErrorCalculated ;

	vTreeNodeIndex.push_back( m_root.m_iTreeNodeIndex ) ;
	vTreeNodeErrors.push_back( glm::vec3( 0 ) ) ;
	vTreeNodeErrorCalculated.push_back( false ) ;

	bool bLoop = true ;
		
	while ( bLoop )
	{
		int iCutSize = (int)vTreeNodeIndex.size() ;
		int iNotBounded = 0 ;
		int iTreeNodeIndex ;

		for ( int i = iCutSize - 1; i >= 0; --i )
		{
			if ( true == vTreeNodeErrorCalculated[ i ] )
			{
				break ;
			}

			iTreeNodeIndex = vTreeNodeIndex[ i ] ;

			light_node_t* pNode = &m_vLightTree[ iTreeNodeIndex ] ;
			light_node_t* pLeft = NULL ;
			light_node_t* pRight = NULL ;

			if ( -1 != pNode->m_iLeftChildNodeIndex )
			{
				pLeft = &m_vLightTree[ pNode->m_iLeftChildNodeIndex ] ;
			}

			if ( -1 != pNode->m_iRightChildNodeIndex )
			{
				pRight = &m_vLightTree[ pNode->m_iRightChildNodeIndex ] ;
			}

			if ( NULL == pLeft && NULL == pRight )
			{
				vTreeNodeErrors[ i ] = glm::vec3( 0 ) ;
				vTreeNodeErrorCalculated[ i ] = true ;
			}
			else
			{
				estimatedL = calculate_L( *pNode, p, n ) ;
				L = glm::vec3( 0 ) ;

				if ( pLeft )
				{
					L += calculate_L( *pLeft, p, n ) ;
				}

				if ( pRight )
				{
					L += calculate_L( *pRight, p, n ) ;
				}

				vError.r = std::fabsf( 1.0f - estimatedL.r / L.r ) ;
				vError.g = std::fabsf( 1.0f - estimatedL.g / L.g ) ;
				vError.b = std::fabsf( 1.0f - estimatedL.b / L.b ) ;

				vTreeNodeErrors[ i ] = vError ;
				vTreeNodeErrorCalculated[ i ] = true ;
			}
		}

		for ( int i = 0; i < iCutSize; ++i )
		{
			if ( vTreeNodeErrors[ i ].r > kBoundError || vTreeNodeErrors[ i ].g > kBoundError || vTreeNodeErrors[ i ].b > kBoundError )
			{
				++iNotBounded ;	// TODO : break here
				break ;
			}
		}

		// found light cut
		if ( 0 == iNotBounded || iCutSize >= kMaxLightsInCut )
		{
			//std::cout << "( x: " << x << ", y: " << y << " ) - cut: " << iCutSize << std::endl ;

			bLoop = false ;
			break ;
		}

		// go to deeper

		// find node which has large error and is not leaf node
		int iIndex = -1 ;
		float fMaxError = 0 ;
		float fError ;
			
		int iNodeSize = (int)vTreeNodeErrors.size() ;
		for ( int i = 0; i < iNodeSize; ++i )
		{
			iTreeNodeIndex = vTreeNodeIndex[ i ] ;

			if ( false == m_vLightTree[ iTreeNodeIndex ].m_bLeafNode )
			{
				fError = vTreeNodeErrors[ i ].r + vTreeNodeErrors[ i ].g + vTreeNodeErrors[ i ].b ;

				if ( -1 == iIndex || fMaxError < fError )
				{
					iIndex = i ;
					fMaxError = fError ;
				}
			}
		}

		// replace cut with children
		if ( -1 != iIndex )
		{
			iTreeNodeIndex = vTreeNodeIndex[ iIndex ] ;

			int iLeftTreeNodeIndex = m_vLightTree[ iTreeNodeIndex ].m_iLeftChildNodeIndex ;
			int iRightTreeNodeIndex = m_vLightTree[ iTreeNodeIndex ].m_iRightChildNodeIndex ;

			// add left child
			if ( -1 != iLeftTreeNodeIndex )
			{
				vTreeNodeIndex.push_back( iLeftTreeNodeIndex ) ;
				vTreeNodeErrors.push_back( glm::vec3( 0 ) ) ;
				vTreeNodeErrorCalculated.push_back( false ) ;
			}

			// add right child
			if ( -1 != iRightTreeNodeIndex )
			{
				vTreeNodeIndex.push_back( iRightTreeNodeIndex ) ;
				vTreeNodeErrors.push_back( glm::vec3( 0 ) ) ;
				vTreeNodeErrorCalculated.push_back( false ) ;
			}

			// remove current node
			vTreeNodeIndex.erase( vTreeNodeIndex.begin() + iIndex ) ;
			vTreeNodeErrors.erase( vTreeNodeErrors.begin() + iIndex ) ;
			vTreeNodeErrorCalculated.erase( vTreeNodeErrorCalculated.begin() + iIndex ) ;
		}
	}

	// make light cut per sample
	int iCutSize = (int)vTreeNodeIndex.size() ;
	for ( int i = 0; i < iCutSize; ++i )
	{
		vLightcut.push_back( m_vLightTree[ vTreeNodeIndex[ i ] ] ) ;
		vLightcut[ i ].m_fError = ( vTreeNodeErrors[ i ].r + vTreeNodeErrors[ i ].g + vTreeNodeErrors[ i ].b ) / 3.0f ;
	}

	return vLightcut ;
}


int		lightcuts_t::get_child_node_used_count( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, float& fOutError )
{
	if ( vUsedTreeNodeError.size() > 0 )	fOutError = vUsedTreeNodeError[ node.m_iTreeNodeIndex ] ;
	else									fOutError = 0.0f ;

	int iNodeUsed = vUsedTreeNodeCount[ node.m_iTreeNodeIndex ] ;
	if ( node.m_bLeafNode )
	{
		return iNodeUsed ;
	}

	float fLeftError, fRightError ;

	int iLeftUsed = get_child_node_used_count( m_vLightTree[ node.m_iLeftChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fLeftError ) ;
	int iRightUsed = get_child_node_used_count( m_vLightTree[ node.m_iRightChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fRightError ) ;
	int iAvgUsed = (int)( ( iLeftUsed + iRightUsed ) / 2.0f + 0.5f ) ;
	float fAvgError = ( fLeftError + fRightError ) / 2.0f ;

	fOutError = std::max( fOutError, fAvgError ) ;

	return std::max( iNodeUsed, iAvgUsed ) ;
}

int		lightcuts_t::get_child_node_max_used_count( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, float& fOutMaxError )
{
	if ( node.m_bLeafNode )
	{
		fOutMaxError = 0.0f ;
		return vUsedTreeNodeCount[ node.m_iTreeNodeIndex ] ;
	}

	if ( vUsedTreeNodeError.size() > 0 )	fOutMaxError = vUsedTreeNodeError[ node.m_iTreeNodeIndex ] ;
	else									fOutMaxError = 0.0f ;

	int iNodeUsed = vUsedTreeNodeCount[ node.m_iTreeNodeIndex ] ;

	float fLeftMaxError, fRightMaxError ;

	int iLeftUsed = get_child_node_used_count( m_vLightTree[ node.m_iLeftChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fLeftMaxError ) ;
	int iRightUsed = get_child_node_used_count( m_vLightTree[ node.m_iRightChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fRightMaxError ) ;
	int iAvgUsed = (int)( ( iLeftUsed + iRightUsed ) / 2.0f + 0.5f ) ;
	float fAvgError = ( fLeftMaxError + fRightMaxError ) / 2.0f ;

	fOutMaxError = std::max( fOutMaxError, fAvgError ) ;

	return std::max( iNodeUsed, iAvgUsed ) ;
}

void	lightcuts_t::add_valid_node( const light_node_t& node, const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, int kMaxLightsInCut, int iLevel )
{
	m_vLightCut.push_back( node ) ;

	int iNodeUsed = vUsedTreeNodeCount[ node.m_iTreeNodeIndex ] ;
	float fError = 0.0f ;
	if ( vUsedTreeNodeError.size() > 0 )
	{
		fError = vUsedTreeNodeError[ node.m_iTreeNodeIndex ] ;
	}

	float fLeftMaxError = 0.0f ;
	float fRightMaxError = 0.0f ;

	int iLeftMaxUsed = 0 ;
	int iRightMaxUsed = 0 ;

	if ( -1 != node.m_iLeftChildNodeIndex )
	{
		iLeftMaxUsed = get_child_node_max_used_count( m_vLightTree[ node.m_iLeftChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fLeftMaxError ) ;
	}

	if ( -1 != node.m_iRightChildNodeIndex )
	{
		iRightMaxUsed = get_child_node_max_used_count( m_vLightTree[ node.m_iRightChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, fRightMaxError ) ;
	}

	int iAvgNodeNumInLevel = std::pow( 2, iLevel ) ;
	float fAvgMaxError = ( fLeftMaxError + fRightMaxError ) / 2.0f ;

	//if ( false == vUsedTreeNodeError.empty() && fError <= kBoundError && fError >= ( fLeftMaxError + fRightMaxError ) / 2.0f )
	if ( false == vUsedTreeNodeError.empty() && fError <= kBoundError && fAvgMaxError * iAvgNodeNumInLevel <= kBoundError / 2.0f )
	{
		if ( 0 == fError && 0 == fAvgMaxError && ( iLeftMaxUsed > 0 || iRightMaxUsed > 0 ) )
		{
			// go deeper node
		}
		else
		{
			return ;
		}
	}
/*/
	if ( kMaxLightsInCut > 0 && m_vLightCut.size() >= kMaxLightsInCut )
	{
		return ;
	}
//*/

	if ( iNodeUsed <= 1 || iNodeUsed <= ( iLeftMaxUsed + iRightMaxUsed ) / 2 )
	{
		// remove last item
		m_vLightCut.erase( m_vLightCut.begin() + m_vLightCut.size() - 1 ) ;

		if ( false == node.m_bLeafNode )
		{
			add_valid_node( m_vLightTree[ node.m_iLeftChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, kMaxLightsInCut, iLevel + 1 ) ;
			add_valid_node( m_vLightTree[ node.m_iRightChildNodeIndex ], vUsedTreeNodeCount, vUsedTreeNodeError, kMaxLightsInCut, iLevel + 1 ) ;
		}
	}
}

void	lightcuts_t::make_light_cut( const std::vector< int >& vUsedTreeNodeCount, const std::vector< float >& vUsedTreeNodeError, int kMaxLightsInCut )
{
	add_valid_node( m_root, vUsedTreeNodeCount, vUsedTreeNodeError, kMaxLightsInCut, 1 ) ;
}





cube_depth_map_t::cube_depth_map_t( int shadowSize )
	: m_iShadowSize( shadowSize )
{
/*/
	for ( int i = 0; i < 6; ++i )
	{
		m_arrCubeDepthMap[ i ].reserve( shadowSize * shadowSize ) ;
	}
//*/
	
	for ( int i = 0; i < 6; ++i )
	{
		m_arrCubeDepthMap[ i ] = NULL ;
	}
}

cube_depth_map_t::~cube_depth_map_t()
{
	for ( int i = 0; i < 6; ++i )
	{
		if ( m_arrCubeDepthMap[ i ] )
		{
			delete [] m_arrCubeDepthMap[ i ] ;
			m_arrCubeDepthMap[ i ] = NULL ;
		}
	}
}

void	cube_depth_map_t::setCubeDepthMap( GLenum cube_map_pos, float* pTex, std::size_t tex_size )
{
	std::size_t totalSize = m_iShadowSize * m_iShadowSize ;

	if ( !pTex || tex_size < totalSize )
	{
		std::cerr << "invalid cube map texture or texture size ..." << std::endl ;
		return ;
	}

	int iCubeMapIndex = (int)( cube_map_pos - GL_TEXTURE_CUBE_MAP_POSITIVE_X ) ;
	if ( iCubeMapIndex < 0 || iCubeMapIndex > 5 )
	{
		std::cerr << "invalid cube map texture index ..." << std::endl ;
		return ;
	}
/*/
	std::size_t iPass = tex_size / totalSize ;
	for ( size_t i = 0; i < tex_size; )
	{
		m_arrCubeDepthMap[ iCubeMapIndex ].push_back( pTex[ i ] ) ;

		i += iPass ;
	}
//*/

	m_arrCubeDepthMap[ iCubeMapIndex ] = pTex ;
}

float	cube_depth_map_t::getDepth( glm::vec3 vDir ) const
{
	int iCubeMapIndex = -1 ;

	// find maximum component to get cube map texture index
	bool bPositiveX = vDir.x >= 0 ? true : false ;
	bool bPositiveY = vDir.y >= 0 ? true : false ;
	bool bPositiveZ = vDir.z >= 0 ? true : false ;

	float lx = bPositiveX ? vDir.x : -vDir.x ;
	float ly = bPositiveY ? vDir.y : -vDir.y ;
	float lz = bPositiveZ ? vDir.z : -vDir.z ;

	float max = lx > ly ? lx : ly ;
	max = max > lz ? max : lz ;

	// normalize
	vDir /= max ;

	// [-1,1] to [0,1]
	vDir = vDir * 0.5f + 0.5f ;
		
	float u, v ;

	if ( lx == max )
	{
		if ( bPositiveX )
		{
			iCubeMapIndex = 0 ;		// GL_TEXTURE_CUBE_MAP_POSITIVE_X

			u = vDir.z ;
			v = vDir.y ;
		}
		else
		{
			iCubeMapIndex = 1 ;		// GL_TEXTURE_CUBE_MAP_NEGATIVE_X

			u = -vDir.z ;
			v = vDir.y ;
		}
	}
	else if ( ly == max )
	{
		if ( bPositiveY )
		{
			iCubeMapIndex = 2 ;		// GL_TEXTURE_CUBE_MAP_POSITIVE_Y

			u = vDir.x ;
			v = vDir.z ;
		}
		else
		{
			iCubeMapIndex = 3 ;		// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y

			u = vDir.x ;
			v = -vDir.z ;
		}
	}
	else if ( lz == max )
	{
		if ( bPositiveZ )
		{
			iCubeMapIndex = 4 ;		// GL_TEXTURE_CUBE_MAP_POSITIVE_Z

			u = vDir.x ;
			v = vDir.y ;
		}
		else
		{
			iCubeMapIndex = 5 ;		// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z

			u = -vDir.x ;
			v = vDir.y ;
		}
	}

	// TODO : negative value problem
	if ( u < 0 ) u = 1.0f - u ;
	if ( v < 0 ) v = 1.0f - v ;

	// scale to shadow map size
	u *= m_iShadowSize ;
	v *= m_iShadowSize ;

	u = (int)( u + 0.5f ) ;
	v = (int)( v + 0.5f ) ;

	if ( u >= m_iShadowSize )	u = m_iShadowSize - 1.0f ;
	if ( v >= m_iShadowSize )	v = m_iShadowSize - 1.0f ;

	//return m_arrCubeDepthMap[ iCubeMapIndex ][ (int)( v * m_iShadowSize + u ) ] ;
	return m_arrCubeDepthMap[ iCubeMapIndex ][ (int)( v * m_iShadowSize * 3 + u * 3 ) ] ;
}

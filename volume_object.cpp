/*
 * =====================================================================================
 *
 *       Filename:  volume_object.cpp  Version:  1.0  Created:  01/16/2015 04:58:12 PM
 *
 *    Description:  volume representation of data
 *
 *         Author:  Bo Wu (Robert), wubo.gfkd@gmail.com
 *	    Copyright:  Copyright (c) 2015, Bo Wu
 *   Organization:  National University of Defense Technology
 *
 * =====================================================================================
 */

#include <iostream>
#include <fstream>
#include <algorithm>
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include "volume_object.h"

VolumeObject::VolumeObject()
{
	if (mesh_name.empty())
	{
		std::cerr<<"mesh_name without initialization \n";
		exit(-1);
	}
	initial_volume();
}

VolumeObject::VolumeObject(std::string name)
{
	mesh_name = name;
	initial_volume();
}

VolumeObject::~VolumeObject()
{
}


///////////////////////////////////////////
//generate volume from mesh
void VolumeObject::initial_volume()
{
	read_mesh();
	grid = openvdb::FloatGrid::create(10.0);
	openvdb::math::Transform::Ptr grid_transform = openvdb::math::Transform::createLinearTransform(0.008);
	grid->setTransform(grid_transform);
	grid->setGridClass(openvdb::GRID_LEVEL_SET);
	grid->setName("mesh_grid");
//	openvdb::tools::MeshToVolume<openvdb::FloatGrid> mesh2volume(grid_transform);
//	mesh2volume.convertToLeveSet(points, triangles);
//	grid = mesh2volume.distGridPtr();
	grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(grid->transform(), points, triangles, float(openvdb::LEVEL_SET_HALF_WIDTH));

} //end of initial_volume


///////////////////////////////////////////
//read_mesh
void VolumeObject::read_mesh()
{
	if(!OpenMesh::IO::read_mesh(mesh, mesh_name))	
	{
		std::cerr<<"read mesh "<<mesh_name<<" error!\n";
		exit(-1);
	}

	// normalize the mesh
	OpenMesh::VertexHandle v_h = mesh.vertex_handle(0);
	MyTraits::Point bb_center, bb_min, bb_max;
	bb_min = bb_max = OpenMesh::vector_cast<MyTraits::Point>(mesh.point(v_h));
	TriMesh::VertexIter v_it ;
	//get bounding box
	for(v_it=mesh.vertices_begin(); v_it!=mesh.vertices_end(); ++v_it)
	{
		bb_min.minimize(OpenMesh::vector_cast<MyTraits::Point>(mesh.point(*v_it)));
		bb_max.maximize(OpenMesh::vector_cast<MyTraits::Point>(mesh.point(*v_it)));
	}
	bb_center = (bb_max + bb_min) / 2.0;
	bb_max = bb_max - bb_min;
	Real scalar_max = std::max(bb_max[0], bb_max[1]);
	scalar_max = std::max(scalar_max, bb_max[2]);
	scalar_max = 1.0 / scalar_max;
	openvdb::Vec3s v_pos;
	for(v_it=mesh.vertices_begin(); v_it!=mesh.vertices_end(); ++v_it)
	{
		//mesh.point(*v_it) = (mesh.point(*v_it) - bb_center) * scalar_max;
		//fill points
		for(int i=0; i < 3; ++i)
		{
			v_pos(i) = mesh.point(*v_it)[i];
		}
		points.push_back(v_pos);
	}

	//fill triangles
	TriMesh::FaceIter f_it;
	TriMesh::FaceVertexIter fv_it;
	openvdb::Vec3I fv_index;
	int counter;
	for(f_it=mesh.faces_begin(); f_it!=mesh.faces_end(); ++f_it)
	{
		counter = 0;
		for(fv_it=mesh.fv_iter(*f_it); fv_it.is_valid(); ++fv_it, ++counter)
		{
			if(counter >= 3)
			{
				std::cerr<<"more that 3 points, not triangle mesh \n";
				exit(-1);
			}
			fv_index(counter) = fv_it->idx();
		}
		triangles.push_back(fv_index);
	}

}

////////////////////////////////////////////
//write grid
void VolumeObject::write_grid(std::string name)
{
	if(name.substr(name.length()-4, 4).compare(".vdb") != 0)
	{
		name = name + ".vdb";
	}
	openvdb::io::File file(name);
	if(file.hasBloscCompression())
	{
		std::cout<<"my openvdb has blosc compression support\n";
	}
	std::cout<<"default compression flags "<<file.DEFAULT_COMPRESSION_FLAGS<<std::endl;
	openvdb::GridPtrVec grids;
	file.setCompression(openvdb::io::COMPRESS_ZIP|openvdb::io::COMPRESS_ACTIVE_MASK);
	grids.push_back(grid);
	file.write(grids);
	file.close();
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  calc_vector_field
 *  Description:  
 * =====================================================================================
 */
void VolumeObject::calc_vector_field()
{
	openvdb::FloatGrid::Accessor accessor = grid->getAccessor();
	std::cout<<"leaf num "<<grid->tree().leafCount()<<std::endl;
	std::cout<<"active leaf voxel "<<grid->tree().activeLeafVoxelCount()<<"\ninactive leaf voxel "<<grid->tree().inactiveLeafVoxelCount()<<"\n";
	int inactive=0, active=0, total=0;
	for(openvdb::FloatGrid::ValueOnCIter iter=grid->cbeginValueOn(); iter.test(); ++iter)	
	{
		active++;
		if(iter.getLevel() != 0)
			std::cout<<"im an active tile\n";
	}
	for(openvdb::FloatGrid::ValueOffIter iter=grid->beginValueOff(); iter.test(); ++iter)
	{
		if(iter.getLevel() != 0)
			continue;
		else
			inactive++;
		/* 
		if(iter.getLevel() != 0 && accessor)
			std::cout<<"im an inactive tile\n";
			*/
	}
	for(openvdb::FloatGrid::ValueAllIter iter=grid->beginValueAll(); iter.test(); ++iter)
	{
		total++;
		/*
		if(iter.getLevel() != 0)
			std::cout<<"im an total tile\n";
			*/
	}
	std::cout<<"my active is "<< active <<"\ninactive is "<<inactive<<std::endl;
	std::cout<<"total num "<<total<<std::endl;
}		/* -----  end of function calc_vector_field  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  save_as_mesh
 *  Description:  convert volume grid as mesh
 * =====================================================================================
 */
void VolumeObject::save_as_mesh()
{
    std::vector<openvdb::Vec3s> output_points;
    std::vector<openvdb::Vec3I> output_tris;
    std::vector<openvdb::Vec4I> output_quad;
    openvdb::tools::volumeToMesh(*grid, output_points, output_tris, output_quad, 0.0);
    std::string suffix = "_merge.obj";
    auto output_name = mesh_name.substr(0, mesh_name.size()-4) + suffix;
    std::ofstream mesh_file(output_name);
    if(mesh_file.is_open())
    {
        mesh_file << "#output from volume grid\n#author: Bo Wu\n#vertices: "<<output_points.size()<<"\n#quad: "<<output_quad.size()<<"\n#tris: "<<output_tris.size()<<std::endl;
        for(auto p : output_points)
        {
            mesh_file<<"v ";
            for(int i=0; i < 3; ++i)
            {
                mesh_file<<p(i)<<" ";
            }
            mesh_file<<std::endl;
        }
        mesh_file<<std::endl;
        for(auto f : output_quad)
        {
            mesh_file<<"f ";
            for(int i=3; i >= 0; --i)
            {
                mesh_file<<f(i)+1<<" ";
            }
            mesh_file<<std::endl;
        }
    }
    mesh_file.close();
}		/* -----  end of function save_as_mesh  ----- */

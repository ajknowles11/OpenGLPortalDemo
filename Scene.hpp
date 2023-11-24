#pragma once

/*
 * A scene manages a hierarchical arrangement of transformations (via "Transform").
 *
 * Each transformation may have associated:
 *  - Drawing data (via "Drawable")
 *  - Camera information (via "Camera")
 *  - Light information (via "Light")
 *
 */

#include "GL.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gl_compile_program.hpp"

#include <list>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

struct Scene {
	struct Transform {
		//Transform names are useful for debugging and looking up locations in a loaded scene:
		std::string name;

		//The core function of a transform is to store a transformation in the world:
		glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); //n.b. wxyz init order
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);

		//The transform above may be relative to some parent transform:
		Transform *parent = nullptr;

		//It is often convenient to construct matrices representing this transformation:
		// ..relative to its parent:
		glm::mat4x3 make_local_to_parent() const;
		glm::mat4x3 make_parent_to_local() const;
		// ..relative to the world:
		glm::mat4x3 make_local_to_world() const;
		glm::mat4x3 make_world_to_local() const;

		//since hierarchy is tracked through pointers, copy-constructing a transform  is not advised:
		Transform(Transform const &) = delete;
		//if we delete some constructors, we need to let the compiler know that the default constructor is still okay:
		Transform() = default;

		Transform(glm::vec3 const &position_, glm::quat const &rotation_, glm::vec3 const &scale_) : position(position_), rotation(rotation_), scale(scale_) {}

		//make a global transform representation from an existing transform. Really helpful for rendering purposes (probably not good elsewhere)
		void make_global(Transform const &from);
	};

	struct Drawable {
		//a 'Drawable' attaches attribute data to a transform:
		Drawable(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;

		//Contains all the data needed to run the OpenGL pipeline:
		struct Pipeline {
			GLuint program = 0; //shader program; passed to glUseProgram

			//attributes:
			GLuint vao = 0; //attrib->buffer mapping; passed to glBindVertexArray

			GLenum type = GL_TRIANGLES; //what sort of primitive to draw; passed to glDrawArrays
			GLuint start = 0; //first vertex to draw; passed to glDrawArrays
			GLuint count = 0; //number of vertices to draw; passed to glDrawArrays

			//uniforms:
			GLuint OBJECT_TO_CLIP_mat4 = -1U; //uniform location for object to clip space matrix
			GLuint OBJECT_TO_LIGHT_mat4x3 = -1U; //uniform location for object to light space (== world space) matrix
			GLuint NORMAL_TO_LIGHT_mat3 = -1U; //uniform location for normal to light space (== world space) matrix

			GLuint CLIP_PLANE_vec4 = -1U; //uniform loc. for clipping plane (used by portals)

			std::function< void() > set_uniforms; //(optional) function to set any other useful uniforms

			//texture objects to bind for the first TextureCount textures:
			enum : uint32_t { TextureCount = 4 };
			struct TextureInfo {
				GLuint texture = 0;
				GLenum target = GL_TEXTURE_2D;
			} textures[TextureCount];
		} pipeline;
	};

	struct Camera {
		//a 'Camera' attaches camera data to a transform:
		Camera(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;
		//NOTE: cameras are directed along their -z axis

		//perspective camera parameters:
		float fovy = glm::radians(60.0f); //vertical fov (in radians)
		float aspect = 1.0f; //x / y
		float near = 0.01f; //near plane
		//computed from the above:
		glm::mat4 make_projection() const;
	};

	struct Light {
		//a 'Light' attaches light data to a transform:
		Light(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;
		//NOTE: directional, spot, and hemisphere lights are directed along their -z axis

		enum Type : char {
			Point = 'p',
			Hemisphere = 'h',
			Spot = 's',
			Directional = 'd'
		} type = Point;

		//light energy convolved with our conventional tristimulus spectra:
		//  (i.e., "red, gree, blue" light color)
		glm::vec3 energy = glm::vec3(1.0f);

		//Spotlight specific:
		float spot_fov = glm::radians(45.0f); //spot cone fov (in radians)
	};

	struct BoxCollider {
		BoxCollider() {}
		BoxCollider(glm::vec3 min_, glm::vec3 max_) : min(min_), max(max_) {}
		~BoxCollider() {}
		glm::vec3 min = glm::vec3(0);
		glm::vec3 max = glm::vec3(0);
	};

	struct Portal {
		Portal() : active(false) {}
		Portal(Drawable *drawable_, BoxCollider box_, std::string on_walkmesh_) : Portal(drawable_, box_, on_walkmesh_, nullptr) {}
		Portal(Drawable *drawable_, BoxCollider box_, std::string on_walkmesh_, Portal *dest_) : drawable(drawable_), box(box_.min + glm::vec3(0,-0.25f, 0), box_.max + glm::vec3(0,0.25f,0)), on_walkmesh(on_walkmesh_), dest(dest_) {}
		~Portal() {}
		Drawable *drawable = nullptr;
		Portal *dest = nullptr;
		BoxCollider box;
		bool player_in_front = false;
		bool sleeping = false;

		bool active = true;
		std::string on_walkmesh;

		glm::vec4 get_clipping_plane(glm::vec3 view_pos);
	};

	//Scenes, of course, may have many of the above objects:
	std::list< Transform > transforms;
	std::list< Drawable > drawables;
	std::list< Camera > cameras;
	std::list< Light > lights;
	std::unordered_map<std::string, Portal*> portals;

	//The "draw" function provides a convenient way to pass all the things in a scene to OpenGL:
	void draw(Camera const &camera) const;

	//This is the actual recursive portal render function
	void draw(glm::mat4 const &cam_projection, 
		Transform const &cam_transform, 
		glm::vec4 const &clip_plane = glm::vec4(0), 
		GLint max_recursion_lvl = 0, 
		GLint recursion_lvl = 0) const;

	//This helper function draws normal drawables
	void draw_non_portals(glm::mat4 const &world_to_clip, 
		glm::mat4x3 const &world_to_light = glm::mat4x3(1.0f), 
		glm::vec4 const &clip_plane = glm::vec4(0)) const;

	//And this one draws a single drawable
	void draw_one(Drawable const &drawable, glm::mat4 const &world_to_clip, 
		glm::mat4x3 const &world_to_light = glm::mat4x3(1.0f), 
		glm::vec4 const &clip_plane = glm::vec4(0)) const;

	// Draw a tri covering the entire screen. Useful for selective depth buffer operations.
	void draw_fullscreen_tri() const;
	struct FullTriProgram {
		FullTriProgram() {
			program = gl_compile_program(
				"#version 330\n"
				"out vec2 texcoord;\n"
				"void main() {\n"
				"	float x,y;\n"
				"	x = -1.0 + float((gl_VertexID & 1) << 2);\n"
				"	y = -1.0 + float((gl_VertexID & 2) << 1);\n"
				"	texcoord.x = (x + 1.0) * 0.5;\n"
				"	texcoord.y = (y + 1.0) * 0.5;\n"
				"	gl_Position = vec4(x, y, 0.0, 1.0);\n"
				"}\n"
			,
				"#version 330\n"
				"out vec4 fragColor;\n"
				"void main() {\n"
				"	fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
				"}\n"
			);
			glGenVertexArrays(1, &vao);
		}
		GLuint program = 0;
		GLuint vao = 0;
	} full_tri_program;

	//add transforms/objects/cameras from a scene file to this scene:
	// the 'on_drawable' callback gives your code a chance to look up mesh data and make Drawables:
	// throws on file format errors
	void load(std::string const &filename,
		std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable = nullptr,
		std::function< void(Scene &, Transform *, std::string const &, Transform *) > const &on_portal = nullptr
	);

	//this function is called to read extra chunks from the scene file after the main chunks are read:
	// this is useful if you, e.g., subclassing scene to represent a game level/area
	virtual void load_extra(std::istream &from, std::vector< char > const &str0, std::vector< Transform * > const &xfh0) { }

	//empty scene:
	Scene() = default;

	//load a scene:
	Scene(std::string const &filename, std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable,
		std::function< void(Scene &, Transform *, std::string const &, Transform *) > const &on_portal);

	//copy a scene (with proper pointer fixup):
	Scene(Scene const &); //...as a constructor
	Scene &operator=(Scene const &); //...as scene = scene
	//... as a set() function that optionally returns the transform->transform mapping:
	void set(Scene const &, std::unordered_map< Transform const *, Transform * > *transform_map = nullptr);
};

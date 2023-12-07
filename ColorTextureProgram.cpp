#include "ColorTextureProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline color_texture_program_pipeline;

Load< ColorTextureProgram > color_texture_program(LoadTagEarly, []() -> ColorTextureProgram const * {
	ColorTextureProgram *ret = new ColorTextureProgram();

	//----- build the pipeline template -----
	color_texture_program_pipeline.program = ret->program;

	color_texture_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;

	color_texture_program_pipeline.CLIP_PLANE_vec4 = ret->CLIP_PLANE_vec4;
	color_texture_program_pipeline.SELF_CLIP_PLANE_vec4 = ret->SELF_CLIP_PLANE_vec4;

	//make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);


	color_texture_program_pipeline.textures[0].texture = tex;
	color_texture_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

ColorTextureProgram::ColorTextureProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform vec4 CLIP_PLANE;\n"
		"uniform vec4 SELF_CLIP_PLANE;\n"
		"in vec4 Position;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"   gl_ClipDistance[0] = dot(Position, CLIP_PLANE);\n"
		"   gl_ClipDistance[1] = dot(Position, SELF_CLIP_PLANE);"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = texture(TEX, texCoord) * color;\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	CLIP_PLANE_vec4 = glGetUniformLocation(program, "CLIP_PLANE");
	SELF_CLIP_PLANE_vec4 = glGetUniformLocation(program, "SELF_CLIP_PLANE");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

ColorTextureProgram::~ColorTextureProgram() {
	glDeleteProgram(program);
	program = 0;
}


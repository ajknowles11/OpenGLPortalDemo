#include "SolidOutlineProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline solid_outline_program_pipeline;

Load< SolidOutlineProgram > solid_outline_program(LoadTagEarly, []() -> SolidOutlineProgram const * {
	SolidOutlineProgram *ret = new SolidOutlineProgram();

	//----- build the pipeline template -----
	solid_outline_program_pipeline.program = ret->program;

	solid_outline_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;
	solid_outline_program_pipeline.OBJECT_TO_LIGHT_mat4x3 = ret->OBJECT_TO_LIGHT_mat4x3;
	solid_outline_program_pipeline.NORMAL_TO_LIGHT_mat3 = ret->NORMAL_TO_LIGHT_mat3;

	solid_outline_program_pipeline.CLIP_PLANE_vec4 = ret->CLIP_PLANE_vec4;
	solid_outline_program_pipeline.SELF_CLIP_PLANE_vec4 = ret->SELF_CLIP_PLANE_vec4;

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


	solid_outline_program_pipeline.textures[0].texture = tex;
	solid_outline_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

SolidOutlineProgram::SolidOutlineProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform mat4x3 OBJECT_TO_LIGHT;\n"
		"uniform mat3 NORMAL_TO_LIGHT;\n"
		"uniform vec4 CLIP_PLANE;\n"
		"uniform vec4 SELF_CLIP_PLANE;\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	vec3 position = OBJECT_TO_LIGHT * Position;\n"
		"   gl_ClipDistance[0] = dot(vec4(position,1), CLIP_PLANE);\n"
		"   gl_ClipDistance[1] = dot(vec4(position,1), SELF_CLIP_PLANE);"
		"	normal = NORMAL_TO_LIGHT * Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"out vec3 outNormal;\n"
		"out float outDepth;\n"
		"void main() {\n"
		"	fragColor = texture(TEX, texCoord) * color;\n"
		"	outNormal = normalize(normal);\n"
		"	outDepth = gl_FragCoord.z;\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OBJECT_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "OBJECT_TO_LIGHT");
	NORMAL_TO_LIGHT_mat3 = glGetUniformLocation(program, "NORMAL_TO_LIGHT");

	CLIP_PLANE_vec4 = glGetUniformLocation(program, "CLIP_PLANE");
	SELF_CLIP_PLANE_vec4 = glGetUniformLocation(program, "SELF_CLIP_PLANE");

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SolidOutlineProgram::~SolidOutlineProgram() {
	glDeleteProgram(program);
	program = 0;
}

#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D normalTexture;
uniform sampler2D depthTexture;

//fog related
uniform bool fogEnabled;
uniform vec4 fogColor;
uniform float noFogDepth;
uniform float fogDensity;

//https://gist.github.com/Hebali/6ebfc66106459aacee6a9fac029d0115

uniform int width;
uniform int height;

void make_kernel(inout vec4 n[9], sampler2D tex, vec2 coord)
{
	float w = 0.5 / width;
	float h = 0.5 / height;

	n[0] = vec4(texture(tex, coord + vec2( -w, -h)));
	n[1] = vec4(texture(tex, coord + vec2(0.0, -h)));
	n[2] = vec4(texture(tex, coord + vec2(  w, -h)));
	n[3] = vec4(texture(tex, coord + vec2( -w, 0.0)));
	n[4] = vec4(texture(tex, coord));
	n[5] = vec4(texture(tex, coord + vec2(  w, 0.0)));
	n[6] = vec4(texture(tex, coord + vec2( -w, h)));
	n[7] = vec4(texture(tex, coord + vec2(0.0, h)));
	n[8] = vec4(texture(tex, coord + vec2(  w, h)));
}

bool smoothDepth(vec4 n[9], float threshold){
	float center = n[4].r;
	float sum = 0;
	sum += abs(n[0].r + n[1].r + n[2].r + n[3].r + n[5].r + n[6].r + n[7].r + n[8].r - (8.0 * center));
	return sum < threshold;
}

void main()
{
    vec3 col = texture(screenTexture, TexCoords).rgb;
    vec3 normal = texture(normalTexture, TexCoords).rgb;
    float depth = texture(depthTexture, TexCoords).r;

	//Apply sobel filter
	vec4 n[9];

	make_kernel( n, depthTexture, TexCoords );
	vec4 depth_sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
  	vec4 depth_sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
	vec4 depth_sobel = sqrt((depth_sobel_edge_h * depth_sobel_edge_h) + (depth_sobel_edge_v * depth_sobel_edge_v));
	bool smoothD = smoothDepth(n, 0.009);

	make_kernel(n, normalTexture, TexCoords);
	vec4 normal_sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
  	vec4 normal_sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
	vec4 normal_sobel = sqrt((normal_sobel_edge_h * normal_sobel_edge_h) + (normal_sobel_edge_v * normal_sobel_edge_v));

	float depth_threshold = 0.002f;
	float normal_threshold = 0.1f;

	float maxRGB = max(col.r, max(col.g, col.b));
	float minRGB = min(col.r, min(col.g, col.b));
	float lum = 0.5 * (maxRGB + minRGB);
	float sat = (maxRGB - minRGB) / (1.0 - abs(2.0 * lum - 1.0));
	// if(sat < 0.25){
	// 	col = vec3(1);
	// }
	FragColor = (normal_sobel.x > normal_threshold || !smoothD && (depth_sobel.x > depth_threshold))? vec4(vec3(0.1), 1.0) : vec4(col,1);

	// Apply fog

	//now the depth buffer clears to 0.8, weirldly
	if(depth == 0.8){
		depth = 15.0;
	}

	if(fogEnabled){
		if(depth > noFogDepth){
		//Linear Fog
		//FragColor = mix(FragColor, fogColor, clamp((depth - noFogDepth) / (14.5 - noFogDepth), 0.0, 1.0));

		//Exponential (Squared) Fog
		float scaledDepth = mix(0, 5, clamp((depth - noFogDepth) / (15 - noFogDepth), 0, 1));
		float scaledDepth2 = scaledDepth * scaledDepth;
		float fogCoeff = 1.0/ pow(fogDensity, scaledDepth2);

		FragColor = mix(fogColor, FragColor, fogCoeff);
		}
	}
} 
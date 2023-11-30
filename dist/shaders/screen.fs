#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D normalTexture;
uniform sampler2D depthTexture;



//https://gist.github.com/Hebali/6ebfc66106459aacee6a9fac029d0115

uniform int width;
uniform int height;

void make_kernel(inout vec4 n[9], sampler2D tex, vec2 coord)
{
	float w = 1.0 / width;
	float h = 1.0 / height;

	n[0] = vec4(texture2D(tex, coord + vec2( -w, -h)));
	n[1] = vec4(texture2D(tex, coord + vec2(0.0, -h)));
	n[2] = vec4(texture2D(tex, coord + vec2(  w, -h)));
	n[3] = vec4(texture2D(tex, coord + vec2( -w, 0.0)));
	n[4] = vec4(texture2D(tex, coord));
	n[5] = vec4(texture2D(tex, coord + vec2(  w, 0.0)));
	n[6] = vec4(texture2D(tex, coord + vec2( -w, h)));
	n[7] = vec4(texture2D(tex, coord + vec2(0.0, h)));
	n[8] = vec4(texture2D(tex, coord + vec2(  w, h)));
}


float near = 0.1; 
float far  = 10.0; 
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main()
{
    vec3 col = texture(screenTexture, TexCoords).rgb;
    vec3 normal = texture(normalTexture, TexCoords).rgb;
    float depth = texture(depthTexture, TexCoords).r;
    depth = LinearizeDepth(depth) / far;


    FragColor = vec4(vec3(depth), 1.0f);

	//Apply sobel filter
	vec4 n[9];
	make_kernel( n, depthTexture, TexCoords );

	vec4 depth_sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
  	vec4 depth_sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
	vec4 depth_sobel = sqrt((depth_sobel_edge_h * depth_sobel_edge_h) + (depth_sobel_edge_v * depth_sobel_edge_v));

	make_kernel(n, screenTexture, TexCoords);
	vec4 color_sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
  	vec4 color_sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
	vec4 color_sobel = sqrt((color_sobel_edge_h * color_sobel_edge_h) + (color_sobel_edge_v * color_sobel_edge_v));

	make_kernel(n, normalTexture, TexCoords);
	vec4 normal_sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
  	vec4 normal_sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
	vec4 normal_sobel = sqrt((normal_sobel_edge_h * normal_sobel_edge_h) + (normal_sobel_edge_v * normal_sobel_edge_v));


	vec4 ourSobel = vec4(max(depth_sobel.r, normal_sobel.r), 
	max(depth_sobel.g, normal_sobel.g),
	max(depth_sobel.b, normal_sobel.b),
	1.0);

    float ourSobelMax = max(ourSobel.r, max(ourSobel.g, ourSobel.b));
	float sobel_threshold = 0.0015f;
	FragColor = (depth_sobel.x > sobel_threshold || ourSobelMax > 0.5)? vec4(1, 0.6, 0.6, 1.0) : vec4(col, 0);
	//FragColor = mix(vec4(col, 0.0), vec4(1), ourSobel);

    //FragColor = vec4(col, 1.0);
    //FragColor = vec4(normal, 1.0);//normal
    
    //FragColor = vec4(vec3(LinearizeDepth(depth)), 1.0);//depth
} 
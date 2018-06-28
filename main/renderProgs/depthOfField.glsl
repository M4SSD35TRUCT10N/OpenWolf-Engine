/*[Vertex]*/
attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;

uniform mat4		u_ModelViewProjectionMatrix;

uniform vec4		u_ViewInfo; // zfar / znear, zfar
uniform vec2		u_Dimensions;
uniform vec4		u_Local0; // viewAngX, viewAngY, 0, 0

varying vec2		var_TexCoords;
varying vec4		var_ViewInfo; // zfar / znear, zfar
varying vec2		var_Dimensions;
varying vec4		var_Local0; // viewAngX, viewAngY, 0, 0

void	main()
{
	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * vec4(attr_Position, 1.0);
	var_TexCoords = attr_TexCoord0.st;
	var_ViewInfo = u_ViewInfo;
	var_Dimensions = u_Dimensions.st;
	var_Local0 = u_Local0.rgba;
}

/*[Fragment]*/
uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_DepthMap;

varying vec2		var_TexCoords;
varying vec4		var_ViewInfo; // zfar / znear, zfar
varying vec2		var_Dimensions;
varying vec4		var_Local0; // viewAngX, viewAngY, 0, 0

void	main()
{
	//gl_FragColor = texture2D(u_DiffuseMap, var_TexCoords.st);
	//return;

	// calculate the screen texcoord in the 0.0 to 1.0 range
	//vec2 st = var_TexCoords.st * r_FBufScale;
	//vec2 st = var_TexCoords.st / var_Dimensions.st;
	vec2 st = var_TexCoords.st;

	// scale by the screen non-power-of-two-adjust
	//st *= r_NPOTScale;

	float focus = 0.98;			// focal distance, normalized 0.8-0.999
	//float radius = 0.5;	  	// 0 - 20.0
	float radius = 5.0;	  	// 0 - 20.0

	vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);

	// autofocus
	focus = texture2D(u_DepthMap, vec2(0.5, 0.5)).r;

	const float tap = 5.0;
	const float taps = tap * 2.0 + 1.0;

	float depth = texture2D(u_DepthMap, st).r;
	float delta = (abs(depth - focus) * abs(depth - focus)) / float(tap);
	delta *= radius;
	//delta = clamp(radius * delta, -max, max);

	for(float i = -tap; i < tap; i++)
    {
	    for(float j = -tap; j < tap; j++)
	    {
			sum += texture2D(u_DiffuseMap, st + ((vec2(i, j) * delta) / var_Dimensions.st));
		}
	}

	sum *=  1.0 / (taps * taps);

	gl_FragColor = sum;
}

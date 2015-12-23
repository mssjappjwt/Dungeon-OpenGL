#version 120

varying vec3 fPosition; // get the interpolated value from the vertex shader
varying vec3 fNormal;   // get the interpolated value from the vertex shader
varying vec2 fTexture;  // get the interpolated value from the vertex shader

uniform sampler2D texture; // texture unit to sample from

uniform mat4 modelview_matrix;
uniform mat4 view_matrix;

// lighting stuff for 2 lights
uniform vec4 AmbientProd[2], DiffuseProd[2], SpecularProd[2], LightPosition[2];
uniform vec3 spotDirection;
uniform float shininess;

void main() 
{
	vec3 posInCam = (modelview_matrix * vec4(fPosition, 1.0)).xyz; // fragment position in camera space
	vec3 V = normalize(-posInCam); // direction to the eye in camera space
	vec3 N = normalize((modelview_matrix * vec4(fNormal, 0.0)).xyz); // fragment normal in camera space
	vec4 totalColor = vec4(0.0);

	// for each light
	for(int i = 0; i < 2; i++)
	{
		vec3 lightInCam = (view_matrix * LightPosition[i]).xyz; // light position in camera space
		vec3 L = normalize(lightInCam-posInCam); // direction to the light
		vec3 H = normalize(L+V); // half-vector

		// ambient light contribution
		vec4 ambient = AmbientProd[i];
		
		// diffuse light contribution
		float Kd = max(dot(L,N), 0.0);
		vec4 diffuse = Kd*DiffuseProd[i];

		// specular light contribution
		vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);
		if(dot(L,N) > 0.0)
		{
			float Ks = pow(max(dot(N,H), 0.0), shininess);
			specular = Ks*SpecularProd[i];
		}

		// combined contributions
		vec4 color = ambient + diffuse + specular;

		// if this is the flashlight
		if(i == 1)
		{
			// spot direction in camera space
			vec3 sd = normalize((view_matrix * vec4(spotDirection, 0.0)).xyz);

			// flashlight brightness decreases with angle from spot direction
			color *= clamp(30.0 * (dot(sd, -L) - 0.95), 0.0, 1.0);
		}

		totalColor += color;
	}

	totalColor.a = 1.0;

	vec4 texColor = texture2D(texture, fTexture); // get the texture color at location fTexture
	gl_FragColor = totalColor * texColor; // apply the color and texture to the fragment
}

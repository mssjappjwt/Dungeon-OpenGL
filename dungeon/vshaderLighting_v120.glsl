#version 120

// vertex attributes (position, normal, texture coordinates)
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexture;

varying vec3 fPosition; // to send to the fragment shader, interpolated along the way
varying vec3 fNormal;   // to send to the fragment shader, interpolated along the way
varying vec2 fTexture;  // to send to the fragment shader, interpolated along the way

uniform mat4 modelview_matrix; // model matrix to transpose vertices from object coord to world coord
uniform mat4 proj_matrix;      // projection matrix
uniform mat4 view_matrix;      // view matrix

void main() 
{
	// assign the vertex position to the vPosition attribute multiplied by the matrices
  	gl_Position = proj_matrix * modelview_matrix * vec4(vPosition, 1.0);

	// send to the fragment shader
	fPosition = vPosition;
	fNormal = vNormal;
  	fTexture = vTexture;
}

#include <stdio.h>
#include "Angel.h"  // includes gl.h, glut.h and other stuff...
#include "glm.h"

// material parameters
struct Material
{
	vec4 diffuse, ambient, specular;
	float shininess;
};

// light parameters
struct Light
{
	vec4 position;
	vec4 diffuse, ambient, specular;
};

// object data
class Object
{
public:
	bool visible;      // object visibility
	vec3* vertices;    // vertex positions
	vec3* normals;     // vertex normals
	vec2* texcoords;   // vertex texture coordinates
	int nVertices;     // actual number of vertices
	Material material; // object material
	GLuint buffer;     // buffer ID
	mat4 matrix;       // local object transformation
	GLuint texture;    // texture IDs
	Object *next;      // next object in scene graph hierarchy
	Object *children;  // child objects in scene graph hierarchy

	Object() : visible(true), vertices(NULL), normals(NULL), texcoords(NULL), nVertices(0), buffer(0), texture(0), next(NULL), children(NULL)
	{
	}

	void addChild(Object* child)
	{
		child->next = children;
		children = child;
	}
};

// scene graph hierarchy
class SceneGraph
{
public:
	Object *root;

	SceneGraph()
	{
		root = new Object;
	}

	~SceneGraph()
	{
		deleteObjects(root);
	}

	void deleteObjects(Object* object)
	{
		// traverse the scene graph 
		while(object)
		{
			// delete object's children recursively
			deleteObjects(object->children);

			glDeleteBuffers(1, &object->buffer);

			// delete the object and move to the next child
			Object *next = object->next;
			delete object;
			object = next;
		}
	}
};

// objects
SceneGraph sceneGraph;
vec4 spotPosition;
vec3 chestPosition(-4, 0, 4);
GLuint program;  // shader ID

// pointers to some objects for individual control
Object* ground = NULL;
Object* person = NULL;
Object* flashlight = NULL;
Object* room = NULL;
Object* chest = NULL;

// texture stuff
const int nTextures = 7; // number of textures for objects
const char* filenames[nTextures] = {"data/ground.ppm", "data/building.ppm", "data/person.ppm", "data/flashlight.ppm", "data/room.ppm", "data/barrel.ppm", "data/chest.ppm"};
GLuint textures[nTextures]; // texture IDs

// camera stuff
float yawAngle = 45, pitchAngle = 0;
vec3 viewPoint(0, 1.2f, -15), viewDirection;
mat4 viewMatrix, projMatrix;

// toggles
bool interiorScene = false;
bool firstPersonView = false;
bool flashlightEnabled = true;
bool chestPicked = false;
bool explorationMode = false;

// exploration stuff
int mouseX = 0, mouseY = 0; // mouse position
mat4 explorationMatrix;

// forward declarations of functions
void init();
void display();
void resize(int width, int height);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void close();

int main(int argc, char **argv)
{
    glutInit(&argc, argv);	// initialize glut
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);  // set display mode to use a double RGBA color framebuffer and a depth buffer
    glutInitWindowSize(800, 600); // set window size

    glutCreateWindow("Dungeon"); // open the window

	// initialize glew if necessary (don't need to on Macs)
	#ifndef __APPLE__
	GLenum err = glewInit();
	#endif

    init();

	// set up the callback functions
    glutDisplayFunc(display);   // what to do when it's time to draw
    glutKeyboardFunc(keyboard); // what to do if a keyboard event is detected
	glutSpecialFunc(special);   // what to do if a special key event is detected
	glutMouseFunc(mouse);       // what to do if a mouse click event is detected
	glutMotionFunc(motion);     // what to do if a mouse drag event is detected
	glutWMCloseFunc(close);     // what to do at the end
	glutReshapeFunc(resize);    // use for recomputing projection matrix on reshape
    glutMainLoop();             // start infinite loop, listening for events
    return 0;
}

// setting of the buffer data
void setBuffers(Object* object, GLMmodel* model)
{
	// allocate memory for buffers
	object->nVertices = 3 * model->numtriangles;
	object->vertices = (vec3*)malloc(sizeof(*object->vertices) * object->nVertices);
	object->normals = (vec3*)malloc(sizeof(*object->normals) * object->nVertices);
	object->texcoords = (vec2*)malloc(sizeof(*object->texcoords) * object->nVertices);

	// copy vertices to buffers
	for(int i = 0; i < (int)model->numtriangles; i++) 
	{
		_GLMtriangle T = model->triangles[i];
		for(int j = 0; j < 3; j++) 
		{
			float x, y, z;

			// vertex position
			x = model->vertices[3*T.vindices[j] + 0];
			y = model->vertices[3*T.vindices[j] + 1];
			z = model->vertices[3*T.vindices[j] + 2];
			object->vertices[3*i+j] = vec3(x, y, z);

			// vertex normal
			x = model->normals[3*T.nindices[j] + 0];
			y = model->normals[3*T.nindices[j] + 1];
			z = model->normals[3*T.nindices[j] + 2];
			object->normals[3*i+j] = vec3(x, y, z);

			// vertex texture coordinates
			x = model->texcoords[2*T.tindices[j] + 0];
			y = model->texcoords[2*T.tindices[j] + 1];
			object->texcoords[3*i+j] = vec2(x, y);
		}
	}

	// create the buffer
	glGenBuffers(1, &object->buffer);

	// sizes of vertex/normal/texcoord buffers
	int vsize = sizeof(*object->vertices) * object->nVertices;
	int nsize = sizeof(*object->normals) * object->nVertices;
	int tsize = sizeof(*object->texcoords) * object->nVertices;

	// move the vertex data to the buffer
	glBindBuffer(GL_ARRAY_BUFFER, object->buffer);
	glBufferData(GL_ARRAY_BUFFER, vsize + nsize + tsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vsize, object->vertices);
	glBufferSubData(GL_ARRAY_BUFFER, vsize, nsize, object->normals);
	glBufferSubData(GL_ARRAY_BUFFER, vsize + nsize, tsize, object->texcoords);
}

// setting of the vertex attributes
void setAttributes(Object* object)
{
	glBindBuffer(GL_ARRAY_BUFFER, object->buffer);

	// sizes of vertex/normal buffers
	int vsize = sizeof(*object->vertices) * object->nVertices;
	int nsize = sizeof(*object->normals) * object->nVertices;

	// link the position attribute data
	GLuint vPosition_loc = glGetAttribLocation(program, "vPosition");
	glEnableVertexAttribArray(vPosition_loc);
	glVertexAttribPointer(vPosition_loc, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	// link the normal attribute data
	GLuint vNormal_loc = glGetAttribLocation(program, "vNormal");
	glEnableVertexAttribArray(vNormal_loc);
	glVertexAttribPointer(vNormal_loc, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(vsize));

	// link the texture coordinate attribute data
	GLuint vTexture_loc = glGetAttribLocation(program, "vTexture");
	glEnableVertexAttribArray(vTexture_loc);
	glVertexAttribPointer(vTexture_loc, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(vsize + nsize));
}

// setting of the object lighting
void setLighting(Object* object)
{
	// set up the general light
	Light light0;
	light0.ambient = vec4(0.5f, 0.5f, 0.5f, 1);  // ambient color
	light0.diffuse = vec4(0.5f, 0.5f, 0.5f, 1);  // diffuse color
	light0.specular = vec4(0.5f, 0.5f, 0.5f, 1); // specular color
	light0.position = vec4(0, 1, 0, 0);          // light position in world coordinates

	// set up the spot light
	Light light1;
	light1.ambient = vec4(0.5f, 0.5f, 0.5f, 1);  // ambient color
	light1.diffuse = vec4(0.5f, 0.5f, 0.5f, 1);  // diffuse color
	light1.specular = vec4(0.5f, 0.5f, 0.5f, 1); // specular color
	light1.position = vec4(spotPosition.x, spotPosition.y, spotPosition.z, 1); // spot position in world coordinates

	// shininess
	GLuint shininess_loc = glGetUniformLocation(program, "shininess");
	glUniform1f(shininess_loc, object->material.shininess);

	// lighting variables for the light0
	GLuint AP_loc = glGetUniformLocation(program, "AmbientProd[0]");
	GLuint DP_loc = glGetUniformLocation(program, "DiffuseProd[0]");
	GLuint SP_loc = glGetUniformLocation(program, "SpecularProd[0]");
	GLuint LP_loc = glGetUniformLocation(program, "LightPosition[0]");
	glUniform4fv(AP_loc, 1, light0.ambient * object->material.ambient);
	glUniform4fv(DP_loc, 1, light0.diffuse * object->material.diffuse);
	glUniform4fv(SP_loc, 1, light0.specular * object->material.specular);
	glUniform4fv(LP_loc, 1, light0.position);

	// lighting variables for the light1
	AP_loc = glGetUniformLocation(program, "AmbientProd[1]");
	DP_loc = glGetUniformLocation(program, "DiffuseProd[1]");
	SP_loc = glGetUniformLocation(program, "SpecularProd[1]");
	LP_loc = glGetUniformLocation(program, "LightPosition[1]");
	GLuint SD_loc = glGetUniformLocation(program, "spotDirection");
	float flash = flashlightEnabled && !explorationMode ? 1.0f : 0.0f;
	glUniform4fv(AP_loc, 1, light1.ambient * object->material.ambient * flash);
	glUniform4fv(DP_loc, 1, light1.diffuse * object->material.diffuse * flash);
	glUniform4fv(SP_loc, 1, light1.specular * object->material.specular * flash);
	glUniform4fv(LP_loc, 1, light1.position);
	glUniform3fv(SD_loc, 1, viewDirection);
}

// object loading
void loadObject(Object* object, char* filename)
{
	// load the model and compute the normals
	GLMmodel* model = glmReadOBJ(filename);
	glmFacetNormals(model);
	glmVertexNormals(model, object == person ? 90.0f : 0.0f); // smooth normals for the person only

	// create the vertex buffers
	setBuffers(object, model);

	// data in system memory is no longer needed
	free(object->vertices);
	free(object->normals);
	free(object->texcoords);
	glmDelete(model);

	// set the object material
	object->material.ambient = vec4(1, 1, 1, 1);
	object->material.diffuse = vec4(1, 1, 1, 1);
	object->material.specular = vec4(1, 1, 1, 1);
	object->material.shininess = 100;
}

// program initialization
void init()
{
	// create the ground object and add it to the scene graph
	ground = new Object;
	loadObject(ground, "data/ground.obj");
	ground->texture = 0;
	sceneGraph.root->addChild(ground);

	// create the building object and add it to the scene graph
	Object* building = new Object;
	loadObject(building, "data/building.obj");
	building->texture = 1;
	ground->addChild(building);

	// create the person object and add it to the scene graph
	person = new Object;
	loadObject(person, "data/person.obj");
	person->texture = 2;
	sceneGraph.root->addChild(person);

	// create the flashlight object and add it to the scene graph
	flashlight = new Object;
	loadObject(flashlight, "data/flashlight.obj");
	flashlight->texture = 3;
	flashlight->matrix = Translate(-0.27f, 0.76f, 0) * RotateY(-90);
	person->addChild(flashlight);

	// create the room object and add it to the scene graph
	room = new Object;
	loadObject(room, "data/room.obj");
	room->texture = 4;
	sceneGraph.root->addChild(room);

	// create the barrel object and add it to the scene graph
	Object* barrel = new Object;
	loadObject(barrel, "data/barrel.obj");
	barrel->texture = 5;
	room->addChild(barrel);

	// create the chest object and add it to the scene graph
	chest = new Object;
	loadObject(chest, "data/chest.obj");
	chest->texture = 6;
	chest->matrix = Translate(chestPosition);
	chest->visible = false;
	sceneGraph.root->addChild(chest);

	// load the textures
	for(int i = 0; i < nTextures; i++)
	{
		// create the texture ID
		glGenTextures(1, &textures[i]);
		glBindTexture(GL_TEXTURE_2D, textures[i]);

		// set the texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

		// load the texture
		int width, height;
		GLubyte *data = glmReadPPM((char*)filenames[i], &width, &height);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); // move the data onto the GPU
		free(data);  // don't need this data now that its on the GPU
	}

	// enable the texturing
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glUniform1i(glGetUniformLocation(program, "texture"), 0); // use texture unit #0 for the shader variable "texture"

	// load the shader
	program = InitShader("vshaderLighting_v120.glsl", "fshaderLighting_v120.glsl");
	glUseProgram(program);

	glEnable(GL_DEPTH_TEST); // enable the Z-buffer depth test
	glEnable(GL_CULL_FACE);  // enable culling of back-facing surfaces
}

// scene graph drawing
void drawObjects(Object* object, mat4 matrix, bool visible)
{
	// traverse the scene graph
	while(object)
	{
		// update the flashlight position for lighting
		if(object == flashlight)
		{
			spotPosition = matrix * object->matrix * vec4(0.3f, 0, 0, 1);
		}

		// only if parent and current objects are visible
		if(visible && object->visible)
		{
			// draw the object
			setAttributes(object);
			setLighting(object);
			GLuint modelViewMatrix_loc = glGetUniformLocation(program, "modelview_matrix");
			glUniformMatrix4fv(modelViewMatrix_loc, 1, GL_TRUE, viewMatrix * matrix * object->matrix);
			if(object->texture >= 0 && object->texture < nTextures)
			{
				glBindTexture(GL_TEXTURE_2D, textures[object->texture]);
			}
			glDrawArrays(GL_TRIANGLES, 0, object->nVertices);
		}

		// draw object's children recursively
		drawObjects(object->children, matrix * object->matrix, visible && object->visible);

		// move to the next object in scene graph hierarchy
		object = object->next;
	}
}

// window drawing
void display(void)
{
	float time = glutGet(GLUT_ELAPSED_TIME) * 0.001f; // current time in seconds

	// scene selection
	if(interiorScene)
	{
		// if near the wall
		if(fabs(viewPoint.x) > 7 || fabs(viewPoint.z) > 7)
		{
			// go out
			interiorScene = false;
			if(!chestPicked) chest->visible = false;
		}
	}
	else
	{
		// if near the wall
		if(fabs(viewPoint.x) < 5 && fabs(viewPoint.z) < 5)
		{
			// go in
			interiorScene = true;
			if(!chestPicked) chest->visible = true;
		}
	}

	// object visibility according to current scene
	ground->visible = !interiorScene;
	room->visible = interiorScene;
	person->visible = !firstPersonView;

	// update the person matrix and view direction
	person->matrix = Translate(viewPoint + vec3(0, -1.2f, 0)) * RotateY(yawAngle);
	viewDirection = vec3(sinf(yawAngle * DegreesToRadians), 0, cosf(yawAngle * DegreesToRadians));

	// pick the chest if near
	if(!chestPicked && interiorScene)
	{
		float dx = chestPosition.x - viewPoint.x;
		float dz = chestPosition.z - viewPoint.z;
		if(dx*dx + dz*dz < 1) chestPicked = true;
	}

	// show the picked chest rotated above the person
	if(chestPicked)
	{
		chest->matrix = Translate(viewPoint + viewDirection * 0.0f + vec3(0, 0.8f, 0)) * RotateY(time * 50.0f);
	}

	float introDuration = 8;
	bool introSequence = time < introDuration;

	// camera mode selection
	if(introSequence)
	{
		// camera flying down at start
		vec3 pos0(-10, 15, 15);                    // start camera position
		vec3 pos1 = viewPoint - viewDirection * 2; // end camera position
		float t = time / introDuration;           // flying progress in the range 0..1
		t = (3 - 2 * t) * t * t;                   // smoothing
		vec3 pos = pos0 * (1 - t) + pos1 * t;      // interpolation from pos0 to pos1
		viewMatrix = LookAt(pos, viewPoint, vec3(0, 1, 0));
	}
	else if(firstPersonView)
	{
		// first person point of view
		viewMatrix = RotateX(pitchAngle) * LookAt(viewPoint, viewPoint + viewDirection, vec3(0, 1, 0));
	}
	else
	{
		// third person point of view
		viewMatrix = RotateX(pitchAngle) * LookAt(viewPoint - viewDirection * 2, viewPoint, vec3(0, 1, 0));
	}

	// set uniform values in shader
	GLuint projMatrix_loc = glGetUniformLocation(program, "proj_matrix");
	GLuint viewMatrix_loc = glGetUniformLocation(program, "view_matrix"); // send this in separately to go from just world-->cam
	glUniformMatrix4fv(projMatrix_loc, 1, GL_TRUE, projMatrix);
	glUniformMatrix4fv(viewMatrix_loc, 1, GL_TRUE, viewMatrix);

	// clear the window
	if(explorationMode) glClearColor(0.50f, 0.45f, 0.40f, 1.0f); // background color
		else glClearColor(0.3f, 0.2f, 0.2f, 1.0f); // background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear out the color of the framebuffer and the depth info from the depth buffer

	if(explorationMode && chest)
	{
		// draw the chest
		setAttributes(chest);
		setLighting(chest);
		GLuint modelViewMatrix_loc = glGetUniformLocation(program, "modelview_matrix");
		glUniformMatrix4fv(modelViewMatrix_loc, 1, GL_TRUE, Translate(0, 0, -1.5f) * explorationMatrix * Translate(0, -0.2f, 0));
		if(chest->texture >= 0 && chest->texture < nTextures)
		{
			glBindTexture(GL_TEXTURE_2D, textures[chest->texture]);
		}
		glDrawArrays(GL_TRIANGLES, 0, chest->nVertices);
	}
	else
	{
		// draw the scene graph
		drawObjects(sceneGraph.root, mat4(), true);
	}

	// update the window
	glFlush();
	glutSwapBuffers();
	glutPostRedisplay();
}

// window resizing
void resize(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	projMatrix = Perspective(60.0, GLfloat(w)/h, 0.1f, 100.0f);  // do perspective projection
}

// key handling
void keyboard(unsigned char key, int x, int y)
{
	switch(key)
	{
	case 'q': case 'Q':
		// quit the program
		exit(EXIT_SUCCESS);
		break;
	case 033:
		// stop exploration on escape
		explorationMode = false;
		break;
	case 'A': case 'a':
		// rotate up/down
		if(!explorationMode) pitchAngle += 3;
		break;
	case 'Z': case 'z':
		// rotate up/down
		if(!explorationMode) pitchAngle -= 3;
		break;
	case ' ':
		// toggle first/third person point of view
		if(!explorationMode) firstPersonView = !firstPersonView;
		break;
	case 'F': case 'f':
		// toggle the flashlight
		if(!explorationMode) flashlightEnabled = !flashlightEnabled;
		break;
	case 'I': case 'i':
		if(!explorationMode && chestPicked) chest->visible = !chest->visible;
		break;
	}

	// refresh the window
	glutPostRedisplay();
}

// special key handling
void special(int key, int x, int y)
{
	// store the viewpoint
	vec3 viewPoint0 = viewPoint;

	switch(key)
	{
	case GLUT_KEY_UP:
		// travel forward/backward
		if(!explorationMode) viewPoint += viewDirection * 0.1f;
		break;
	case GLUT_KEY_DOWN:
		// travel forward/backward
		if(!explorationMode) viewPoint -= viewDirection * 0.1f;
		break;
	case GLUT_KEY_LEFT:
		// rotate left/right
		if(!explorationMode) yawAngle += 3;
		break;
	case GLUT_KEY_RIGHT:
		// rotate left/right
		if(!explorationMode) yawAngle -= 3;
		break;
	}

	// restore the viewpoint if too close to the barrel
	if(viewPoint.x*viewPoint.x + viewPoint.z*viewPoint.z < 1)
	{
		viewPoint = viewPoint0;
	}

	// refresh the window
	glutPostRedisplay();
}

// mouse click handling
void mouse(int button, int state, int x, int y)
{
	// if left mouse button is clicked
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		// if inside the building or alredy picked the chest
		if(!explorationMode && (interiorScene || chestPicked))
		{
			// start the exploration
			explorationMode = true;
			explorationMatrix = mat4();
		}

		// update the mouse position
		mouseX = x;
		mouseY = y;
	}

	// if right mouse button is clicked
	if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
		explorationMode = false;
	}

	// refresh the window
	glutPostRedisplay();
}

// mouse drag handling
void motion(int x, int y)
{
	if(explorationMode)
	{
		// mouse displacement at dragging
		float shiftX = (x - mouseX) * 0.4f;
		float shiftY = (y - mouseY) * 0.4f;

		// rotate the explored object
		explorationMatrix = RotateY(shiftX) * explorationMatrix;
		explorationMatrix = RotateX(shiftY) * explorationMatrix;
	}

	// update the mouse position
	mouseX = x;
	mouseY = y;

	// refresh the window
	glutPostRedisplay();
}

// finalization
void close()
{
}

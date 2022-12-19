#include <iostream>
#include <string>
#include <GL/glut.h>
#include "Game.h"

using namespace std;

// CONSTANTES ///////////////////////////////////

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WINDOW_POSITION_X = 300;
const int WINDOW_POSITION_Y = 400;
const char TITLE[] = "PARTE 1";

const GLclampf RED = 1.0;
const GLclampf GREEN = 0.6;
const GLclampf BLUE = 0.5;
const GLclampf ALPHA = 1.0;

// VARIABLES ////////////////////////////////////

bool fullScreenMode = false;
float colorFog[] = { 1.0, 0.6, 0.5, 1.0 };

// USANDO GAME //////////////////////////////////

Game game;

// FUNCIONES OPENGL /////////////////////////////

void initGraphics()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP);
	glFogf(GL_FOG_DENSITY, 0.02);
	glFogfv(GL_FOG_COLOR, colorFog);
	glClearColor(RED, GREEN, BLUE, ALPHA);
	glutSetCursor(GLUT_CURSOR_NONE);
	game.Init();
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	game.Render();

	glutSwapBuffers();
}

//  FUNCIONES AUXILIARES ////////////////////////

void writeLine(string text)
{
	cout << text << endl;
}

void reshape(GLsizei width, GLsizei height)
{
	if (height == 0) height = 1;
	GLfloat aspectRatio = (GLfloat)width / (GLfloat)height;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION),
		glLoadIdentity();
	gluPerspective(60.0f, aspectRatio, 1.0f, 200.0f);
	glMatrixMode(GL_MODELVIEW);
}

void idle()
{
	game.Update();
	glutPostRedisplay();
}

void keyPressed(unsigned char key, int px, int py)
{
	game.ProcessKeyPressed(key, px, py);
	glutPostRedisplay();
}

void keyReleased(unsigned char key, int px, int py)
{
	game.ProcessKeyReleased(key, px, py);
	glutPostRedisplay();
}

void specialKey(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_F11:
		fullScreenMode = !fullScreenMode;
		if (fullScreenMode)
		{
			glutFullScreen();
		}
		else
		{
			glutReshapeWindow(WINDOW_WIDTH, WINDOW_HEIGHT);
			glutPositionWindow(WINDOW_POSITION_X, WINDOW_POSITION_Y);
		}
		break;
	}
}

void mouseMoved(int x, int y)
{
	game.ProcessMouseMovement(x, y);
	glutPostRedisplay();
}

void mouseClicked(int button, int state, int x, int y)
{
	game.ProcessMouseClick(button, state, x, y);
	glutPostRedisplay();
}


// MAIN /////////////////////////////////////////

int main(int argc, char** argv)
{
	writeLine("Starting Weird Engine...");

	glutInit(&argc, argv);											// Inicializa Glut
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);		// doble buffer, modo rgba, depth buffer
	glutInitWindowSize(800, 600);									// tamaño inicial de la ventana
	glutInitWindowPosition(100, 100);								// posición inicial ventana
	glutCreateWindow("title");										// crea una ventana con título dado

	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyPressed);
	glutKeyboardUpFunc(keyReleased);
	glutSpecialFunc(specialKey);
	glutPassiveMotionFunc(mouseMoved);
	glutMouseFunc(mouseClicked);
	glutIdleFunc(idle);

	initGraphics();

	glutMainLoop();
}


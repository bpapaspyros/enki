/*
    Enki - a fast 2D robot simulator
    Copyright (C) 1999-2008 Stephane Magnenat <stephane at magnenat dot net>
    Copyright (C) 2004-2005 Markus Waibel <markus dot waibel at epfl dot ch>
    Copyright (c) 2004-2005 Antoine Beyeler <abeyeler at ab-ware dot com>
    Copyright (C) 2005-2006 Laboratory of Intelligent Systems, EPFL, Lausanne
    Copyright (C) 2006-2008 Laboratory of Robotics Systems, EPFL, Lausanne
    See AUTHORS for details

    This program is free software; the authors of any publication 
    arising from research using this software are asked to add the 
    following reference:
    Enki - a fast 2D robot simulator
    http://lis.epfl.ch/enki
    Stephane Magnenat <stephane at magnenat dot net>,
    Markus Waibel <markus dot waibel at epfl dot ch>
    Laboratory of Intelligent Systems, EPFL, Lausanne.

    You can redistribute this program and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "objects/Objects.h"
#include "Viewer.h"
#include "Viewer.moc"
#include <enki/robots/e-puck/EPuck.h>
#include <enki/robots/alice/Alice.h>
#include <QApplication>
#include <QtGui>

/*!	\file Viewer.cpp
	\brief Implementation of the Qt-based viewer widget
*/

void initTexturesResources()
{
	Q_INIT_RESOURCE(textures);
}

//! Asserts a dynamic cast.	Similar to the one in boost/cast.hpp
template<typename Derived, typename Base>
inline Derived polymorphic_downcast(Base base)
{
	Derived derived = dynamic_cast<Derived>(base);
	assert(derived);
	return derived;
}

namespace Enki
{
	#define rad2deg (180 / M_PI)
	#define clamp(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))
	
	// simple display list, one per instance
	class SimpleDisplayList : public ViewerWidget::ViewerUserData
	{
	public:
		GLuint list;
	
	public:
		SimpleDisplayList()
		{
			list = glGenLists(1);
			deletedWithObject = true;
		}
		
		virtual void draw(PhysicalObject* object) const
		{
			glColor3d(object->color.components[0], object->color.components[1], object->color.components[2]);
			glCallList(list);
		}
		
		virtual ~SimpleDisplayList()
		{
			glDeleteLists(list, 1);
		}
	};
	
	// complex robot, one per robot type stored here
	class CustomRobotModel : public ViewerWidget::ViewerUserData
	{
	public:
		QVector<GLuint> lists;
		QVector<GLuint> textures;
	
	public:
		CustomRobotModel()
		{
			deletedWithObject = false;
		}
	};
	
	class EPuckModel : public CustomRobotModel
	{
	public:
		EPuckModel(ViewerWidget* viewer)
		{
			textures.resize(2);
			textures[0] = viewer->bindTexture(QPixmap(QString(":/textures/epuck.png")), GL_TEXTURE_2D);
			textures[1] = viewer->bindTexture(QPixmap(QString(":/textures/epuckr.png")), GL_TEXTURE_2D, GL_LUMINANCE8);
			lists.resize(5);
			lists[0] = GenEPuckBody();
			lists[1] = GenEPuckRest();
			lists[2] = GenEPuckRing();
			lists[3] = GenEPuckWheelLeft();
			lists[4] = GenEPuckWheelRight();
		}
		
		void cleanup(ViewerWidget* viewer)
		{
			for (int i = 0; i < textures.size(); i++)
				viewer->deleteTexture(textures[i]);
			for (int i = 0; i < lists.size(); i++)
				glDeleteLists(lists[i], 1);
		}
		
		virtual void draw(PhysicalObject* object) const
		{
			DifferentialWheeled* dw = polymorphic_downcast<DifferentialWheeled*>(object);
			
			const double wheelRadius = 2.1;
			const double wheelCirc = 2 * M_PI * wheelRadius;
			const double radiosityScale = 1.01;
			static double asd = 0;
			
			glTranslated(0, 0, wheelRadius);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, textures[0]);
			
			glColor3d(1, 1, 1);
			
			glCallList(lists[0]);
			
			glCallList(lists[1]);
			
			//glColor3d(1-object->color.components[0], 1+object->color.components[1], 1+object->color.components[2]);
			glColor3d(0.6+object->color.components[0]-0.3*object->color.components[1]-0.3*object->color.components[2], 0.6+object->color.components[1]-0.3*object->color.components[0]-0.3*object->color.components[2], 0.6+object->color.components[2]-0.3*object->color.components[0]-0.3*object->color.components[1]);
			glCallList(lists[2]);
			
			glColor3d(1, 1, 1);
			
			// wheels
			glPushMatrix();
			glRotated((fmod(dw->leftOdometry, wheelCirc) * 360) / wheelCirc, 0, 1, 0);
			glCallList(lists[3]);
			glPopMatrix();
			
			glPushMatrix();
			glRotated((fmod(dw->rightOdometry, wheelCirc) * 360) / wheelCirc, 0, 1, 0);
			glCallList(lists[4]);
			glPopMatrix();
			
			// shadow
			glBindTexture(GL_TEXTURE_2D, textures[1]);
			glDisable(GL_LIGHTING);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ZERO, GL_SRC_COLOR);
			
			// wheel shadow
			glPushMatrix();
			glScaled(radiosityScale, radiosityScale, radiosityScale);
			glTranslated(0, -0.025, 0);
			glCallList(lists[3]);
			glPopMatrix();
			
			glPushMatrix();
			glScaled(radiosityScale, radiosityScale, radiosityScale);
			glTranslated(0, 0.025, 0);
			glCallList(lists[4]);
			glPopMatrix();
			
			// bottom shadow
			glTranslated(0, 0, -wheelRadius+0.01);
			glBegin(GL_QUADS);
			glTexCoord2f(0.5f, 0.f);
			glVertex2f(-5.f, -5.f);
			glTexCoord2f(0.5f, 0.5f);
			glVertex2f(5.f, -5.f);
			glTexCoord2f(0.f, 0.5f);
			glVertex2f(5.f, 5.f);
			glTexCoord2f(0.f, 0.f);
			glVertex2f(-5.f, 5.f);
			glEnd();
			
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);
			glEnable(GL_LIGHTING);
			
			glDisable(GL_TEXTURE_2D);
		}
		
		virtual void drawSpecial(PhysicalObject* object, int param) const
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDisable(GL_TEXTURE_2D);
			glCallList(lists[0]);
			glDisable(GL_BLEND);
		}
	};
	
	enum ManagedObjectTypes
	{
		OBJECT_EPUCK_MODEL = 0,
		MANAGED_OBJECT_COUNT
	};
	
	ViewerWidget::ViewerWidget(World *world, QWidget *parent) :
		QGLWidget(parent),
		world(world),
		mouseGrabbed(false),
		worldList(0),
		managedObjects(MANAGED_OBJECT_COUNT, 0),
		yaw(-M_PI/2),
		pitch((3*M_PI)/8),
		pos(-world->w * 0.5, -world->h * 0.2),
		altitude(world->h * 0.5)
	{
		initTexturesResources();
	}
	
	ViewerWidget::~ViewerWidget()
	{
		world->disconnectExternalObjectsUserData();
		if (isValid())
		{
			glDeleteLists(worldList, 1);
			deleteTexture (worldTexture);
		}
		for (int i = 0; i < managedObjects.size(); i++)
		{
			if (managedObjects[i])
			{
				managedObjects[i]->cleanup(this);
				delete managedObjects[i];
			}
		}
	}
	
	void ViewerWidget::renderSegment(const Segment& segment, double height)
	{
		Vector v = segment.b - segment.a;
		Vector n = Vector(v.y, -v.x).unitary();
		glNormal3d(n.x, n.y, 0);
		
		glBegin(GL_QUADS);
		glTexCoord2d(0.751390, 0.248609);
		glVertex3d(segment.a.x, segment.a.y, 0);
		glTexCoord2d(0.248609, 0.248609);
		glVertex3d(segment.b.x, segment.b.y, 0);
		glTexCoord2d(0.001739, 0.001739);
		glVertex3d(segment.b.x, segment.b.y, height);
		glTexCoord2d(0.998266, 0.001739);
		glVertex3d(segment.a.x, segment.a.y, height);
		glEnd();
	}
	
	void ViewerWidget::renderWorld()
	{
		const double wallsHeight = 10;
		const double infPlanSize = 3000;
		
		glNewList(worldList, GL_COMPILE);
		
		glNormal3d(0, 0, 1);
		
		glDisable(GL_LIGHTING);
		
		glColor3d(0.8, 0.8, 0.8);
		glBegin(GL_QUADS);
		glVertex3d(-infPlanSize, -infPlanSize, wallsHeight);
		glVertex3d(infPlanSize+world->w, -infPlanSize, wallsHeight);
		glVertex3d(infPlanSize+world->w, 0, wallsHeight);
		glVertex3d(-infPlanSize, 0, wallsHeight);
		
		glVertex3d(-infPlanSize, world->h, wallsHeight);
		glVertex3d(infPlanSize+world->w, world->h, wallsHeight);
		glVertex3d(infPlanSize+world->w, world->h+infPlanSize, wallsHeight);
		glVertex3d(-infPlanSize, world->h+infPlanSize, wallsHeight);
		
		glVertex3d(-infPlanSize, 0, wallsHeight);
		glVertex3d(0, 0, wallsHeight);
		glVertex3d(0, world->h, wallsHeight);
		glVertex3d(-infPlanSize, world->h, wallsHeight);
		
		glVertex3d(world->w, 0, wallsHeight);
		glVertex3d(world->w+infPlanSize, 0, wallsHeight);
		glVertex3d(world->w+infPlanSize, world->h, wallsHeight);
		glVertex3d(world->w, world->h, wallsHeight);
		glEnd();
		
		glEnable(GL_LIGHTING);
		
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, worldTexture);
		glColor3d(1, 1, 1);
		
		// TODO: use world texture if any
		if (world->useWalls)
		{
			glBegin(GL_QUADS);
			glTexCoord2d(0.470722, 0.470722);
			glVertex3d(0, 0, 0);
			glTexCoord2d(0.529278, 0.470722);
			glVertex3d(world->w, 0, 0);
			glTexCoord2d(0.529278, 0.529278);
			glVertex3d(world->w, world->h, 0);
			glTexCoord2d(0.470722, 0.529278);
			glVertex3d(0, world->h, 0);
			glEnd();
			
			/*// ground center
			glBegin(GL_QUADS);
			glTexCoord2d(0.470722, 0.470722);
			glVertex3d(10, 10, 0);
			glTexCoord2d(0.529278, 0.470722);
			glVertex3d(world->w-10, 10, 0);
			glTexCoord2d(0.529278, 0.529278);
			glVertex3d(world->w-10, world->h-10, 0);
			glTexCoord2d(0.470722, 0.529278);
			glVertex3d(10, world->h-10, 0);
			glEnd();
			
			// ground left
			glBegin(GL_QUADS);
			glTexCoord2d(0.751390, 0.248609);
			glVertex3d(0, 0, 0);
			glTexCoord2d(0.248609, 0.248609);
			glVertex3d(world->w, 0, 0);
			glTexCoord2d(0.470722, 0.470722);
			glVertex3d(world->w-10, 10, 0);
			glTexCoord2d(0.529278, 0.470722);
			glVertex3d(10, 10, 0);
			glEnd();*/
			
			renderSegment(Segment(world->w, 0, 0, 0), wallsHeight);
			renderSegment(Segment(world->w, world->h, world->w, 0), wallsHeight);
			renderSegment(Segment(0, world->h, world->w, world->h), wallsHeight);
			renderSegment(Segment(0, 0, 0, world->h), wallsHeight);
		}
		else
		{
			glBegin(GL_QUADS);
			glTexCoord2d(0.470722, 0.470722);
			glVertex3d(0, 0, 0);
			glTexCoord2d(0.529278, 0.470722);
			glVertex3d(world->w, 0, 0);
			glTexCoord2d(0.529278, 0.529278);
			glVertex3d(world->w, world->h, 0);
			glTexCoord2d(0.470722, 0.529278);
			glVertex3d(0, world->h, 0);
			glEnd();
		}
		
		glDisable(GL_TEXTURE_2D);
		
		glEndList();
	}
	
	void ViewerWidget::renderSimpleObject(PhysicalObject *object)
	{
		SimpleDisplayList *userData = new SimpleDisplayList;
		object->userData = userData;
		glNewList(userData->list, GL_COMPILE);
		
		if (object->boundingSurface)
		{
			// TODO: use object texture if any
			size_t segmentCount = object->boundingSurface->size();
			
			// sides
			for (size_t i = 0; i < segmentCount; ++i)
				renderSegment(Segment((*object->boundingSurface)[i], (*object->boundingSurface)[(i+1) % segmentCount] ), object->height);
			
			// top
			glNormal3d(1, 1, 0);
			glBegin(GL_TRIANGLE_FAN);
			for (size_t i = 0; i < segmentCount; ++i)
				glVertex3d((*object->boundingSurface)[i].x, (*object->boundingSurface)[i].y, object->height);
			glEnd();
		}
		else
		{
			GLUquadric * quadratic = gluNewQuadric();
			assert(quadratic);
			
			// sides
			gluCylinder(quadratic, object->r, object->r, object->height, 32, 1);
			
			// top
			glTranslated(0,0,object->height);
			gluDisk(quadratic, 0, object->r, 32, 1);
			
			gluDeleteQuadric(quadratic);
		}
		
		renderObjectHook(object);
		
		glEndList();
	}
	
	//! Called inside the creation of the object display list in local object coordinate
	void ViewerWidget::renderObjectHook(PhysicalObject *object)
	{
		// dir on top of robots
		if (dynamic_cast<Robot*>(object))
		{
			glColor3d(0, 0, 0);
			glBegin(GL_TRIANGLES);
			glVertex3d(2, 0, object->height+0.01);
			glVertex3d(-2, 1, object->height+0.01);
			glVertex3d(-2, -1, object->height+0.01);
			glEnd();
		}
	}
	
	//! Called when object is displayed, after the display list, with the current world matrix
	void ViewerWidget::displayObjectHook(PhysicalObject *object)
	{
	
	}
	
	//! Called when the drawing of the scene is completed.
	void ViewerWidget::sceneCompletedHook()
	{
		/*// overlay debug info
		if (mouseGrabbed)
			renderText(5, 15, QString("Mouse grabbed, yaw: %0, pitch: %1").arg(yaw).arg(pitch));
		*/
	}
	
	void ViewerWidget::initializeGL()
	{
		glClearColor(0.6, 0.7, 1.0, 0.0);
		glClearColor(0.95, 0.95, 0.95, 1.0);
		
		float LightAmbient[] = {0.6, 0.6, 0.6, 1};
		float LightDiffuse[] = {1.2, 1.2, 1.2, 1};
		float defaultColor[] = {0.5, 0.5, 0.5, 1};
		glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiffuse);
		glEnable(GL_LIGHT0);
		
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, defaultColor);
		
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		glEnable(GL_COLOR_MATERIAL);
		
		glShadeModel(GL_SMOOTH);
		glEnable(GL_LIGHTING);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		
		GLfloat density = 0.001;
 		GLfloat fogColor[4] = {0.95, 0.95, 0.95, 1.0};
		glFogi (GL_FOG_MODE, GL_EXP);
		glFogfv (GL_FOG_COLOR, fogColor);
		glFogf (GL_FOG_DENSITY, density);
		glHint (GL_FOG_HINT, GL_NICEST);
		glEnable (GL_FOG);
		
		worldTexture = bindTexture(QPixmap(QString(":/textures/world.png")), GL_TEXTURE_2D);
		worldList = glGenLists(1);
		renderWorld();
		
		startTimer(30);
	}
	
	void ViewerWidget::paintGL()
	{
		// clean screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		float aspectRatio = (float)width() / (float)height();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glFrustum(-1 * aspectRatio, 1 * aspectRatio, -1, 1, 2, 2000);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glRotated(-90, 1, 0, 0);
		glRotated(rad2deg * pitch, 1, 0, 0);
		glRotated(90, 0, 0, 1);
		glRotated(rad2deg * yaw, 0, 0, 1);
		
		glTranslated(pos.x(), pos.y(), -altitude);
		
		float LightPosition[] = {world->w/2, world->h/2, 60, 1};
		glLightfv(GL_LIGHT0, GL_POSITION,LightPosition);
		
		// draw world and all objects
		glCallList(worldList);
		for (World::ObjectsIterator it = world->objects.begin(); it != world->objects.end(); ++it)
		{
			// if required, render this object
			if (!(*it)->userData)
			{
				if (dynamic_cast<EPuck*>(*it))
				{
					// render E-puck
					if (!managedObjects[OBJECT_EPUCK_MODEL])
						managedObjects[OBJECT_EPUCK_MODEL] = new EPuckModel(this);
					(*it)->userData = managedObjects[OBJECT_EPUCK_MODEL];
				}
				else
					renderSimpleObject(*it);
			}
			
			glPushMatrix();
			
			glTranslated((*it)->pos.x, (*it)->pos.y, 0);
			glRotated(rad2deg * (*it)->angle, 0, 0, 1);
			
			ViewerUserData* userData = polymorphic_downcast<ViewerUserData *>((*it)->userData);
			userData->draw(*it);
			displayObjectHook(*it);
			
			glPopMatrix();
		}
		
		sceneCompletedHook();
	}
	
	void ViewerWidget::resizeGL(int width, int height)
	{
		glViewport(0, 0, width, height);
	}
	
	void ViewerWidget::timerEvent(QTimerEvent * event)
	{
		world->step(1./30.);
		updateGL();
	}
	
	void ViewerWidget::mousePressEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::RightButton)
		{
			mouseGrabbed = true;
			mouseGrabPos = event->pos();
		}
	}
	
	void ViewerWidget::mouseReleaseEvent(QMouseEvent * event)
	{
		if (event->button() == Qt::RightButton)
			mouseGrabbed = false;
	}
	
	void ViewerWidget::mouseMoveEvent(QMouseEvent *event)
	{
		if (mouseGrabbed)
		{
			QPoint diff = event->pos() - mouseGrabPos;
			
			if (event->modifiers() & Qt::ShiftModifier)
			{
				pos.rx() += 0.5 * cos(yaw) * (double)diff.y() + 0.5 * sin(yaw) * (double)diff.x();
				pos.ry() += 0.5 * sin(yaw) * -(double)diff.y() + 0.5 * cos(yaw) * (double)diff.x();
			}
			else
			{
				yaw += 0.01 * (double)diff.x();
				pitch = clamp(pitch + 0.01 * (double)diff.y(), -M_PI / 2, M_PI / 2);
			}
			mouseGrabPos = event->pos();
		}
	}
	
	void ViewerWidget::wheelEvent(QWheelEvent * event)
	{
		if (event->modifiers() & Qt::ShiftModifier)
		{
			altitude += (double)event->delta() / 100;
		}
	}
}

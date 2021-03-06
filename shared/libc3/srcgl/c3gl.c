/*
	c3gl.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#if __APPLE__
#define GL_GLEXT_PROTOTYPES
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/glext.h>
#endif
#include <stdio.h>


#include "c3.h"
#include "c3lines.h"
#include "c3sphere.h"
#include "c3light.h"
#include "c3program.h"

#include "c3driver_context.h"

#include "c3gl.h"

#define GLCHECK(_w) {_w; dumpError(#_w);}

static int dumpError(const char * what)
{
	GLenum e;
	int count = 0;
	while ((e = glGetError()) != GL_NO_ERROR) {
		printf("%s: %s\n", what, gluErrorString(e));
		count++;
	}
	return count;
}

int
c3gl_program_load(
		c3program_p p)
{
	if (!p || p->pid || p->log)
		return -1;

	if (p->verbose)
		printf("%s loading %s\n", __func__, p->name->str);
	for (int si = 0; si < p->shaders.count && !p->log; si++) {
		c3shader_p s = &p->shaders.e[si];

		if (p->verbose)
			printf("%s compiling shader %s\n", __func__, s->name->str);

		s->sid = C3APIO(glCreateShader(s->type));
		const GLchar * pgm = s->shader->str;
		glShaderSource(C3APIO_INT(s->sid), 1, &pgm, NULL);

		glCompileShader(C3APIO_INT(s->sid));

		GLint status;
		glGetShaderiv(C3APIO_INT(s->sid), GL_COMPILE_STATUS, &status);

		if (status != GL_FALSE)
			continue;

		GLint infoLogLength;
		glGetShaderiv(C3APIO_INT(s->sid), GL_INFO_LOG_LENGTH, &infoLogLength);

		p->log = str_alloc(infoLogLength);
		glGetShaderInfoLog(C3APIO_INT(s->sid), infoLogLength, NULL, p->log->str);

		fprintf(stderr, "%s compile %s: %s\n", __func__, s->name->str, p->log->str);
		break;
	}
	if (p->log)
		return -1;
    p->pid = C3APIO(glCreateProgram());

	for (int si = 0; si < p->shaders.count && !p->log; si++) {
		c3shader_p s = &p->shaders.e[si];

    	glAttachShader(C3APIO_INT(p->pid), C3APIO_INT(s->sid));
	}
    glLinkProgram(C3APIO_INT(p->pid));

    GLint status;
    glGetProgramiv(C3APIO_INT(p->pid), GL_LINK_STATUS, &status);

	for (int si = 0; si < p->shaders.count && !p->log; si++) {
		c3shader_p s = &p->shaders.e[si];

		glDetachShader(C3APIO_INT(p->pid), C3APIO_INT(s->sid));
		glDeleteShader(C3APIO_INT(s->sid));
    	s->sid = 0;
	}

    if (status == GL_FALSE) {
        GLint infoLogLength;
        glGetProgramiv(C3APIO_INT(p->pid), GL_INFO_LOG_LENGTH, &infoLogLength);

		p->log = str_alloc(infoLogLength);

        glGetProgramInfoLog(C3APIO_INT(p->pid), infoLogLength, NULL, p->log->str);
		fprintf(stderr, "%s link %s: %s\n", __func__, p->name->str, p->log->str);

		goto error;
    }
    for (int pi = 0; pi < p->params.count; pi++) {
    	c3program_param_p pa = &p->params.e[pi];
    	pa->pid = C3APIO(glGetUniformLocation(C3APIO_INT(p->pid), pa->name->str));
    	if (pa->pid == (c3apiobject_t)-1) {
    		fprintf(stderr, "%s %s: parameter '%s' not found\n",
    				__func__, p->name->str, pa->name->str);
    	} else
        	if (p->verbose)
        		printf("%s %s load parameter [%d]'%s'=%d\n", __func__,
        				p->name->str, pi, pa->name->str,
        				C3APIO_INT(pa->pid));
    }

    c3program_purge(p);
    return 0;
error:
	c3program_purge(p);
	if (p->pid)
		glDeleteProgram(C3APIO_INT(p->pid));
	p->pid = 0;
	return -1;
}

static void
_c3_load_pixels(
		c3pixels_p pix)
{
	GLuint mode = pix->rectangle ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D;
	if (!pix->texture) {
		pix->dirty = 1;
		GLuint texID = 0;
		GLCHECK(glEnable(mode));

		glGenTextures(1, &texID);
		//	if (pix->trace)
				printf("%s Creating texture %s %dx%d (id %d)\n",
					__func__, pix->name ? pix->name->str : "", pix->w, pix->h, texID);
		GLCHECK(glBindTexture(mode, texID));
		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pix->row / pix->psize);
		GLCHECK(glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLCHECK(glTexParameteri(mode, GL_TEXTURE_MIN_FILTER,
				pix->rectangle ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR));
//		GLCHECK(glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
//		GLCHECK(glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
//		GLCHECK(glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_CLAMP));
//		GLCHECK(glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_CLAMP));
		if (!pix->rectangle)
			GLCHECK(glTexParameteri(mode, GL_GENERATE_MIPMAP, GL_TRUE));
	#if 1
		GLfloat fLargest;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
		//printf("fLargest = %f\n", fLargest);
		GLCHECK(glTexParameterf(mode, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest));
	#endif
		if (!pix->rectangle)
			GLCHECK(glGenerateMipmap(mode));

		pix->texture = C3APIO(texID);
		pix->dirty = 1;
	}
	if (pix->dirty) {
		pix->dirty = 0;
		if (pix->base) {
			GLCHECK(glBindTexture(mode, C3APIO_INT(pix->texture)));
			static const struct {
				int gltype;
				int glsub;
			} map[] = {
				[C3PIXEL_ARGB] = { .gltype = GL_RGBA8, .glsub = GL_RGBA, },
				[C3PIXEL_RGB] = { .gltype = GL_RGB8, .glsub = GL_RGB,},
				[C3PIXEL_LUMINANCE] = { .gltype = GL_LUMINANCE, .glsub = GL_LUMINANCE,},
				[C3PIXEL_ALPHA] = { .gltype = GL_ALPHA, .glsub = GL_ALPHA,},
			};

			glTexImage2D(mode, 0,
					map[pix->format].gltype,
					pix->w, pix->h, 0,
					map[pix->format].glsub,
					GL_UNSIGNED_BYTE,
					pix->base);
			dumpError("glTexImage2D");
			if (!pix->rectangle)
				GLCHECK(glGenerateMipmap(mode));
			if (!pix->mutable)
				c3pixels_purge(pix);
		}
	}
}

static void
_c3_create_buffer(
		GLuint name,
        GLuint bufferType,
        void * data,
        size_t dataSize,
        GLuint	* out)
{
	glGenBuffers(1, out);

	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, *out));
	GLCHECK(glBufferData(GL_ARRAY_BUFFER,
			dataSize,
	        data,
	        bufferType));
	GLCHECK(glEnableClientState(name));
}

static void
_c3_load_vbo(
		c3geometry_p g)
{
	if (!g->vertice.count)
		return ;
	if (!g->bid) {
		GLuint	vao;
		glGenVertexArrays(1, &vao);
		g->bid = C3APIO(vao);
	}
	glBindVertexArray(C3APIO_INT(g->bid));

	/*
	 * Use 'realloc' on the array as it frees the data, but leaves 'count'
	 * unchanged. We neec that for the vertices and the indexes, the others
	 * can have a straight free()
	 */
	if (!g->vertice.buffer.bid && g->vertice.count) {
		GLuint bid;

		_c3_create_buffer(GL_VERTEX_ARRAY,
				g->vertice.buffer.mutable ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW,
				g->vertice.e, g->vertice.count * sizeof(g->vertice.e[0]),
				&bid);
		glVertexPointer(3, GL_FLOAT, 0, (void*)0);
		g->vertice.buffer.bid = C3APIO(bid);
		if (!g->vertice.buffer.mutable && !g->debug)
			c3vertex_array_realloc(&g->vertice, 0);
		g->vertice.buffer.dirty = 0;
	}
	if (!g->textures.buffer.bid && g->textures.count) {
		GLuint bid;

		_c3_create_buffer(GL_TEXTURE_COORD_ARRAY,
				g->textures.buffer.mutable ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW,
				g->textures.e, g->textures.count * sizeof(g->textures.e[0]),
				&bid);
		glTexCoordPointer(2, GL_FLOAT, 0, (void*)0);
		g->textures.buffer.bid = C3APIO(bid);
		if (!g->textures.buffer.mutable && !g->debug)
			c3tex_array_free(&g->textures);
		g->textures.buffer.dirty = 0;
	}
	if (!g->normals.buffer.bid && g->normals.count) {
		GLuint bid;

		_c3_create_buffer(GL_NORMAL_ARRAY,
				g->normals.buffer.mutable ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW,
				g->normals.e, g->normals.count * sizeof(g->normals.e[0]),
				&bid);
		glNormalPointer(GL_FLOAT, 0, (void*) 0);
		g->normals.buffer.bid = C3APIO(bid);
		if (!g->normals.buffer.mutable && !g->debug)
			c3vertex_array_free(&g->normals);
		g->normals.buffer.dirty = 0;
	}
	if (!g->colorf.buffer.bid && g->colorf.count) {
		GLuint bid;

		_c3_create_buffer(GL_COLOR_ARRAY,
				g->colorf.buffer.mutable ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW,
				g->colorf.e, g->colorf.count * sizeof(g->colorf.e[0]),
				&bid);
		glColorPointer(4, GL_FLOAT, 0, (void*) 0);
		g->colorf.buffer.bid = C3APIO(bid);
		if (!g->colorf.buffer.mutable && !g->debug)
			c3colorf_array_free(&g->colorf);
		g->colorf.buffer.dirty = 0;
	}
	if (!g->indices.buffer.bid && g->indices.count) {
		GLuint bid;
		glGenBuffers(1, &bid);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bid);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        		g->indices.count * sizeof(g->indices.e[0]),
        		g->indices.e,
        		g->indices.buffer.mutable ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		g->indices.buffer.bid = C3APIO(bid);
		if (!g->indices.buffer.mutable && !g->debug)
			c3indices_array_realloc(&g->indices, 0);
		g->indices.buffer.dirty = 0;
	}
}

static void
_c3_geometry_project(
		c3context_p c,
		const struct c3driver_context_t * d,
		c3geometry_p g,
		c3mat4p m)
{
	if (g->mat.texture)
		_c3_load_pixels(g->mat.texture);
	if (g->mat.program)
		c3gl_program_load(g->mat.program);

	switch(g->type.type) {
		case C3_SPHERE_TYPE:
		case C3_TRIANGLE_TYPE:
		case C3_LINES_TYPE:
			g->type.subtype = (c3apiobject_t)GL_TRIANGLES;
			break;
		case C3_TEXTURE_TYPE: {
			if (g->mat.texture)
				g->type.subtype = (c3apiobject_t)GL_TRIANGLE_FAN;
		}	break;
		case C3_LIGHT_TYPE: {
			c3light_p l = (c3light_p)g;
			GLuint lid = GL_LIGHT0 + C3APIO_INT(l->light_id);
			if (l->color.specular.w > 0)
				glLightfv(lid, GL_SPECULAR, l->color.specular.n);
			if (l->color.ambiant.w > 0)
				glLightfv(lid, GL_AMBIENT, l->color.ambiant.n);
		}	break;
		default:
			g->type.subtype = (c3apiobject_t)GL_TRIANGLES;
			break;
	}

	_c3_load_vbo(g);
//	_c3_update_vbo(g);

	glBindVertexArray(0);
	C3_DRIVER_INHERITED(c, d, geometry_project, g, m);
}

void // TODO cleanup
c3_bbox_vertices(
		c3bbox_t * b,
		c3vec3 out[8]);

/*
 * This id the meta function that draws a c3geometry. It looks for normals,
 * indices, textures and so on and call the glDrawArrays
 */
static void
_c3_geometry_draw(
		c3context_p c,
		const struct c3driver_context_t *d,
		c3geometry_p g )
{
	c3mat4 eye = c3mat4_mul(
			&c3context_view_get(g->object->context)->cam.mtx,
			&g->object->world );
	glLoadMatrixf(eye.n);

#if 0
	if (g->name)
		printf("%s draw %s\n", __func__, g->name->str);
	else
		printf("%s draw %p\n", __func__, g);
#endif
	glMaterialfv(GL_FRONT, GL_SHININESS, &g->mat.shininess);

	switch(g->type.type) {
		case C3_LIGHT_TYPE: {
			c3light_p l = (c3light_p)g;
			GLuint lid = GL_LIGHT0 + C3APIO_INT(l->light_id);
			glLightfv(lid, GL_POSITION, l->position.n);
			if (c3context_view_get(c)->type != C3_CONTEXT_VIEW_LIGHT)
				glEnable(lid);
			else
				glDisable(lid);
		}	break;
	}
	if (!g->bid) {
		C3_DRIVER_INHERITED(c, d, geometry_draw, g);
		return;
	}
	glColor4fv(g->mat.color.n);
	dumpError("glColor");

	GLCHECK(glBindVertexArray(C3APIO_INT(g->bid)));

	//glDisable(GL_TEXTURE_2D);
	if (g->mat.texture) {
		GLuint mode = g->mat.texture->rectangle ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D;
		glEnable(mode);
		if (g->mat.texture->trace)
			printf("%s view %d uses texture %s(%d) (%d tex)\n",
					__func__, c->current, g->mat.texture->name->str,
					C3APIO_INT(g->mat.texture->texture),
					g->textures.count);
	//	printf("tex mode %d texture %d\n", g->mat.mode, g->mat.texture);
		dumpError("glEnable texture");
		glBindTexture(mode, C3APIO_INT(g->mat.texture->texture));
		dumpError("glBindTexture");
	}
	if (g->mat.program)
		GLCHECK(glUseProgram(C3APIO_INT(g->mat.program->pid)));

	if (g->indices.buffer.bid) {
		GLCHECK(glDrawElements(C3APIO_INT(g->type.subtype),
				g->indices.count, GL_UNSIGNED_SHORT,
				(void*)NULL /*g->indices.e*/));
	} else {
		glDrawArrays(C3APIO_INT(g->type.subtype), 0, g->vertice.count);
	}
	glBindVertexArray(0);

	if (g->mat.texture)
		glDisable(g->mat.texture->rectangle ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D);
	if (g->mat.program)
		glUseProgram(0);

	if (g->debug) {
		if (g->normals.count) {
			for (int i = 0; i < g->vertice.count; i++) {
				c3vec3 o = g->vertice.e[i];
				glBegin(GL_LINES);
				glColor4f(0.0,0.0,1.0,1.0);
				glVertex3fv(o.n);
				o = c3vec3_add(o, g->normals.e[i]);
				glColor4f(1.0,0.0,0.0,1.0);
				glVertex3fv(o.n);
				glEnd();
			}
		}
	}
	if (0) {	// debug bounding boxes
		c3vec3	v[8];
		c3_bbox_vertices(&g->bbox, v);
		glColor4f(0.0,0.0,1.0,1.0);
		glPointSize(5);
		glBegin(GL_POINTS);
		for (int i = 0; i < 8; i++) {
			glVertex3fv(v[i].n);
		}
		glEnd();
	}
	C3_DRIVER_INHERITED(c, d, geometry_draw, g);
}

const c3driver_context_t c3context_driver = {
		.geometry_project = _c3_geometry_project,
		.geometry_draw = _c3_geometry_draw,
};

const struct c3driver_context_t *
c3gl_getdriver()
{
	return &c3context_driver;
}

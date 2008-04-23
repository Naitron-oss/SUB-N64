/*
Copyright (C) 2003 Sven Olsen
Copyright (C) 2008 nmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//Include our header file.
#include "OGLFreeType.h"

inline int next_p2 ( int a )
{
    int rval=1;
    while(rval<a) rval<<=1;
    return rval;
}

///Create a display list coresponding to the give character.
void makeDList ( FT_Face face, char ch, GLuint list_base, GLuint * tex_base ) 
{
    
    if(FT_Load_Glyph( face, FT_Get_Char_Index( face, ch ), FT_LOAD_DEFAULT )) throw std::runtime_error("FT_Load_Glyph failed");
    
    FT_Glyph glyph;
    if(FT_Get_Glyph( face->glyph, &glyph )) throw std::runtime_error("FT_Get_Glyph failed");
    
    FT_Glyph_To_Bitmap( &glyph, ft_render_mode_normal, 0, 1 );
    FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
    
    FT_Bitmap& bitmap=bitmap_glyph->bitmap;
    
    int width = next_p2( bitmap.width );
    int height = next_p2( bitmap.rows );

	//Allocate memory for the texture data.
    GLubyte* expanded_data = new GLubyte[ 2 * width * height];
    
    for(int j=0; j <height;j++) 
    {
        for(int i=0; i < width; i++)
        {
            expanded_data[2*(i+j*width)]= expanded_data[2*(i+j*width)+1] = (i>=bitmap.width || j>=bitmap.rows) ? 0 : bitmap.buffer[i + bitmap.width*j];
        }
    }


	//Now we just setup some texture paramaters.
    glBindTexture( GL_TEXTURE_2D, tex_base[ch]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height,0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, expanded_data );
    
    delete [] expanded_data;
    
    glNewList(list_base+ch,GL_COMPILE);

    glBindTexture(GL_TEXTURE_2D,tex_base[ch]);
    
    glTranslatef(bitmap_glyph->left,0,0);
    
    glPushMatrix();
        glTranslatef(0,bitmap_glyph->top-bitmap.rows,0);
        
        float x=(float)bitmap.width / (float)width, y=(float)bitmap.rows / (float)height;
        glBegin(GL_QUADS);
            glTexCoord2d(0,0); glVertex2f(0,bitmap.rows);
            glTexCoord2d(0,y); glVertex2f(0,0);
            glTexCoord2d(x,y); glVertex2f(bitmap.width,0);
            glTexCoord2d(x,0); glVertex2f(bitmap.width,bitmap.rows);
        glEnd();
    glPopMatrix();
    glTranslatef(face->glyph->advance.x >> 6 ,0,0);
    
    glEndList();
}

Font::Font(const char * fname, unsigned int h) 
{
    textures = new GLuint[128];

    this->h=h;
    
    FT_Library library;
    if (FT_Init_FreeType( &library ))  fprintf(stderr,"FT_Init_FreeType failed");
    
    FT_Face face;
    
    if (FT_New_Face( library, fname, 0, &face )) fprintf(stderr,"FT_New_Face failed (there is probably a problem with your font file)");
    
    FT_Set_Char_Size( face, h << 6, h << 6, 96, 96);
    
    list_base=glGenLists(128);
    glGenTextures( 128, textures );
    
    for(unsigned char i=0;i<128;i++) makeDList(face,i,list_base,textures);
    
    FT_Done_Face(face);
    
    FT_Done_FreeType(library);
}

Font::~Font() 
{
    glDeleteLists(list_base,128);
    glDeleteTextures(128,textures);
    delete [] textures;
}

inline void pushProjectionMatrix() 
{
    glPushAttrib(GL_TRANSFORM_BIT);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(viewport[0],viewport[2],viewport[1],viewport[3]);
    glPopAttrib();
}

inline void PopProjectionMatrix() 
{
    glPushAttrib(GL_TRANSFORM_BIT);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    glPopAttrib();
}

void glPrint(const Font &ft_font, float x, float y, const char *fmt, ...)  
{
    pushProjectionMatrix();					
	
    GLuint font=ft_font.list_base;
    float h = ft_font.h/.63f;
    
    char text[256];
    va_list	ap;

    if (fmt == NULL) *text=0;
    else 
    {
        va_start(ap, fmt);
        vsprintf(text, fmt, ap);
        va_end(ap);
    }
    
    const char *start_line=text;
    vector<string> lines;

    const char * c = text;;

    for(;*c;c++) 
    {
        if(*c=='\n') 
        {
            string line;
            for(const char *n=start_line;n<c;n++) line.append(1,*n);
            lines.push_back(line);
            start_line=c+1;
        }
    }
    
    if(start_line) 
    {
        string line;
        for(const char *n=start_line;n<c;n++) line.append(1,*n);
        lines.push_back(line);
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);	
    
    glColor4f(1,1,1,1);
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CLIP_PLANE0);
    glDisable(GL_CLIP_PLANE1);
    glDisable(GL_CLIP_PLANE2);
    glDisable(GL_CLIP_PLANE3);
    glDisable(GL_CLIP_PLANE4);
    glDisable(GL_CLIP_PLANE5);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    glListBase(font);

    float modelview_matrix[16];	
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview_matrix);
    
    for(unsigned int i=0;i<lines.size();i++) 
    {
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(x,y-h*i,0);
        glMultMatrixf(modelview_matrix);
        
        glCallLists(lines[i].length(), GL_UNSIGNED_BYTE, lines[i].c_str());
        
        glPopMatrix();
    }

    glPopAttrib();		

    PopProjectionMatrix();
    glColor4f(0,0,0,0);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Copyright (C) 2009 icepir8                                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <gl/gl.h>
//#include <glext.h>
#include "3dmath.h"

void rsp_uc07_vertex();
void rsp_uc07_modifyvertex();
void rsp_uc07_culldl();
void rsp_uc07_branchz();
void rsp_uc07_tri1();
void rsp_uc07_tri2();
void rsp_uc07_quad();
void rsp_uc07_fastcombine();
void rsp_uc07_rdphalf_cont();
void rsp_uc07_rdphalf_2();
void rsp_uc07_line3d();
void rsp_uc07_texture();
void rsp_uc07_popmatrix();
void rsp_uc07_setgeometrymode();
void rsp_uc07_matrix();
void rsp_uc07_moveword();
void rsp_uc07_movemem();
void rsp_uc07_displaylist();
void rsp_uc07_enddl();
void rsp_uc07_rdphalf_1();
void rsp_uc07_setothermode_l();
void rsp_uc07_setothermode_h();

_u32 dwConkerVtxZAddr=0;

void rsp_uc07_special_1()
{
    DebugBox("special_1");
}

void rsp_uc07_special_2()
{
    DebugBox("special_2");
}

void rsp_uc07_special_3()
{
    DebugBox("special_3");
}

void rsp_uc07_dma_io()
{
    DebugBox("dma_io");
}

void rsp_uc07_load_ucode()
{
    DebugBox("load_ucode");
}

void rsp_uc07_noop()
{
    DebugBox("noop");
}

//////////////////////////////////////////////////////////////////////////////
// structure is tricky for changing the endians                             //
//////////////////////////////////////////////////////////////////////////////
typedef struct 
{
    _s16 y;
    _s16 x;
    _u16 flags;
    _s16 z;

    _s16 t;
    _s16 s;

    _u8  a;
    _u8  b;
    _u8  g;
    _u8  r;
} t_vtx_uc7;

_u32 bzaddr;

//////////////////////////////////////////////////////////////////////////////
// LoadVertex                                                               //
//////////////////////////////////////////////////////////////////////////////
void rsp_uc07_vertex()
{
    _u32    a = segoffset2addr(rdp_reg.cmd1);
    _u32     v0, i, n;

    _u32 dwAddr = segoffset2addr(rdp_reg.cmd1);
    _u32 dwVEnd    = (((rdp_reg.cmd0)   )&0xFFF)/2;
    _u32 dwN      = (((rdp_reg.cmd0)>>12)&0xFFF);
    _u32 dwV0       = dwVEnd - dwN;

    n = (rdp_reg.cmd0 >> 12) & 0xfff; //0xFFF;
    v0 = ((rdp_reg.cmd0 & 0xfff) >> 1) - n; //0x7FF) - n;

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_LOADVTX  vertex %i..%i\n",
                 ADDR, CMD0, CMD1,  v0, n-1);

    for(i = 0; i < n; i++)
    {
//      t_vtx_uc7 *vtx = (t_vtx_uc7 *)&pRDRAM[a+(i*sizeof(t_vtx_uc7))];
        _u32 ad = a + (i * sizeof(t_vtx_uc7));

        v0 &= 0x7f;

        rdp_reg.vtx[v0].x     = (float)(_s16)doReadMemHalfWord(ad+0);
        rdp_reg.vtx[v0].y     = (float)(_s16)doReadMemHalfWord(ad+2);
        rdp_reg.vtx[v0].z     = (float)(_s16)doReadMemHalfWord(ad+4);
        rdp_reg.vtx[v0].flags = (_u16)doReadMemHalfWord(ad+6);
        rdp_reg.vtx[v0].s     = (float)(_s16)doReadMemHalfWord(ad+8);
        rdp_reg.vtx[v0].t     = (float)(_s16)doReadMemHalfWord(ad+10);
        rdp_reg.vtx[v0].r     = (_u8)doReadMemByte(ad+12);
        rdp_reg.vtx[v0].g     = (_u8)doReadMemByte(ad+13);
        rdp_reg.vtx[v0].b     = (_u8)doReadMemByte(ad+14);
        rdp_reg.vtx[v0].a     = (_u8)doReadMemByte(ad+15);

        {
            t_vtx *vertex = &rdp_reg.vtx[v0];
            float tmpvtx[4];

            tmpvtx[0] = vertex->x;
            tmpvtx[1] = vertex->y;
            tmpvtx[2] = vertex->z;
            tmpvtx[3] = 1.0f;

            transform_vector(&tmpvtx[0],vertex->x,vertex->y,vertex->z);
            project_vector2(&tmpvtx[0]);

            vertex->x = tmpvtx[0];
            vertex->y = tmpvtx[1];
            vertex->z = tmpvtx[2];
            vertex->w = tmpvtx[3];

            if (0x00020000 & rdp_reg.geometrymode)
            {
                ////math_lightingN((t_vtx*)(char*)vertex, vertex->lcolor);
                float r = (rdp_reg.light[rdp_reg.ambient_light].r);
                float g = (rdp_reg.light[rdp_reg.ambient_light].g);
                float b = (rdp_reg.light[rdp_reg.ambient_light].b);
                float a = (rdp_reg.light[rdp_reg.ambient_light].a);
                int k;
                for(k=0; k<(int)rdp_reg.lights; k++)
                {
                    r += rdp_reg.light[k].r;
                    g += rdp_reg.light[k].g;
                    b += rdp_reg.light[k].b;
                    a += rdp_reg.light[k].a;
                }
                if( r>1 ) r=1;
                if( g>1 ) g=1;
                if( b>1 ) b=1;
                if( a>1 ) a=1;

                r *= vertex->r ;
                g *= vertex->g ;
                b *= vertex->b ;
                a *= vertex->a ;

                vertex->lcolor[3] = a / 255;
                vertex->lcolor[0] = r / 255;
                vertex->lcolor[1] = g / 255;
                vertex->lcolor[2] = b / 255;
            }
            else
            {
                vertex->lcolor[3] = (float)vertex->a / 255;
                vertex->lcolor[0] = (float)vertex->r / 255;
                vertex->lcolor[1] = (float)vertex->g / 255;
                vertex->lcolor[2] = (float)vertex->b / 255;
            }

            if ((0x00060000 & rdp_reg.geometrymode) == 0x00060000)
            {
                // G_TEXTURE_GEN enable the automatic generation of the texture
                // coordinates s and t.  A spherical mapping is used, based on the normal.
                tmpvtx[0] = (float)(_s8)doReadMemByte((v0<<1) + 0 + dwConkerVtxZAddr);
                tmpvtx[1] = (float)(_s8)doReadMemByte((v0<<1) + 1 + dwConkerVtxZAddr);
                tmpvtx[2] = (float)(_s8)doReadMemByte((v0<<1) + 2 + dwConkerVtxZAddr);

                transform_normal(&tmpvtx[0]);

                vertex->n1 = tmpvtx[0];
                vertex->n2 = tmpvtx[1];
                vertex->n3 = tmpvtx[2];


                vertex->s = (0.5f + (0.5f * vertex->n1));
                vertex->t = (0.5f - (0.5f * vertex->n2));
            }
            else
            {
                int xdsd=0;
            }

            LOG_TO_FILE("\tvtx[%02i]: -> %12.5f %12.5f %12.5f\n"
                        "\t              %i, %i, %i, %i\n",
                v0, vertex->x, vertex->y, vertex->z,
                vertex->r, vertex->g, vertex->b,vertex->a
                );
        }

        v0++;
    }
}

static unsigned char tbuffer[512*256*4];
extern byte bmpHdr[];
extern int bmpcnt;

void rsp_uc07_fullscreen()
{
    unsigned long i, j;
    unsigned int width = 256;
    unsigned int height =256;
    unsigned int mode = 2;

    _u32 *add = (_u32*)(&pRDRAM[segoffset2addr(rdp_reg.cmd1)]);
    _u32 Scrwidth = add[0] / 4;
    _u32 Scrheight = add[2] / 4;
    _u32 fladd = segoffset2addr(add[4]);

    _u16 *src;
    _u16 *base_src_ptr;

//  _u32 *lsrc;
    _u32 *lbase_src_ptr;

    unsigned char *dest;
    _u32 tmpTex = 0x0800001;

    //GLint hadDepthTest, hadBlending;

//  sprintf(output,"uCode = %i\nwidth = %8i\nheight = %8i\nAdd = %8x\ntadd  = %8x",ucode_version,Scrwidth,Scrheight,segoffset2addr(rdp_reg.cmd1),fladd);
//  MessageBox(hGraphics, output, "fixme", MB_OK);

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_FULLSCREEN",ADDR, CMD0, CMD1);
    LOG_TO_FILE("\tAddress = \n",segoffset2addr(rdp_reg.cmd1));
/**/
    return;
/*
    glGetIntegerv(GL_DEPTH_TEST, &hadDepthTest); //** Should be glGetBooleanv, but 
    glDisable(GL_DEPTH_TEST);
    glGetIntegerv(GL_BLEND, &hadBlending); //** Should be glGetBooleanv, but 
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Scrwidth, Scrheight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    glEnable(GL_TEXTURE_2D);
*/
    base_src_ptr  = (_u16*)(&pRDRAM[fladd]);
    lbase_src_ptr = (_u32*)(&pRDRAM[fladd]);

    for (i = 0; i<256*512*4; i++) tbuffer[i] = 0;
//      for (uly = 0; (uly * 256)<Scrheight; uly++)
//      {

        for(i=0; i<Scrheight; i++)
        {
            _u16 *tsrc = &(base_src_ptr[i * Scrwidth]);
            dest = (unsigned char*)&tbuffer[i * 512 * 4];
            for(j=0; j<Scrwidth; j++)
            {               
                src = &tsrc[(j ^ 0x01)];
                *(dest) = (((*src >> 11 ) & 0x001f)<<3); dest++;
                *(dest) = (((*src >>  6 ) & 0x001f)<<3); dest++;
                *(dest) = (((*src >>  1 ) & 0x001f)<<3); dest++;
                *(dest) = 0xff;                          dest++;  //** Alpha

            }
        }

        DumpBmp(tbuffer,512,256);
/*
        glBindTexture(GL_TEXTURE_2D, tmpTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 256, 0, GL_RGBA, 
        GL_UNSIGNED_BYTE, tbuffer);

        glBegin(GL_QUADS);
            glTexCoord2i(0  , 0);     glVertex2i(0  ,0);
            glTexCoord2i(0  , 239);   glVertex2i(0  ,239);          
            glTexCoord2i(319, 239);   glVertex2i(319,239);
            glTexCoord2i(319, 0);     glVertex2i(319,0);
        glEnd();
        glDeleteTextures(1, &tmpTex);
        } // end for ulx uly

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    if (hadBlending) glEnable(GL_BLEND);
    if (hadDepthTest) glEnable(GL_DEPTH_TEST);
*/
}

void rsp_uc07_modifyvertex()
{
    DebugBox("ModifyVertex\n");
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_MODIFYVTX \n", ADDR, CMD0, CMD1);

}

void rsp_uc07_culldl()
{
    DebugBox("CullDisplaylist\n");
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_CULLDL\n", ADDR, CMD0, CMD1);
}

void rsp_uc07_branchz()
{

    rdp_reg.pc_i++;
    if(rdp_reg.pc_i > RDP_STACK_SIZE - 1)
    {
        return;
    }

    rdp_reg.pc[rdp_reg.pc_i] = bzaddr;

    DebugBox("BranchZ\n");
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_BRANCHZ\n", ADDR, CMD0, CMD1);
}


void rsp_uc07_tri1()
{
    int     vn[3];

    vn[0] = ((CMD0 >> 17) & 0x7f);
    vn[1] = ((CMD0 >>  9) & 0x7f);
    vn[2] = ((CMD0 >>  1) & 0x7f);

    DrawVisualTriangle(vn);
//  Render_tr(vn);

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_TRI1 (%i,%i,%i)\n", 
                 ADDR, CMD0, CMD1, vn[0], vn[1], vn[2]);
} // void rsp_uc07_tri1()


void rsp_uc07_tri2()
{
    int     vn[6];
    
    vn[0] = ((CMD0 >> 17) & 0x7f);
    vn[1] = ((CMD0 >>  9) & 0x7f);
    vn[2] = ((CMD0 >>  1) & 0x7f);

    vn[3] = ((CMD1 >> 17) & 0x7f);
    vn[4] = ((CMD1 >>  9) & 0x7f);
    vn[5] = ((CMD1 >>  1) & 0x7f);

    DrawVisualTriangle(vn);
    DrawVisualTriangle(vn+3);

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_TRI2 (%i,%i,%i) (%i,%i,%i)\n", 
                 ADDR, CMD0, CMD1, vn[0], vn[1], vn[2], vn[3], vn[4], vn[5]);
} // void rsp_uc07_tri2()


void rsp_uc07_tri4()
{
    int     vn[12];

    vn[0] = (CMD1>>10)&0x1F;
    vn[1] = (CMD1>> 5)&0x1F;
    vn[2] = (CMD1    )&0x1F;
    
    vn[3] = (CMD1>>25)&0x1F;
    vn[4] = (CMD1>>20)&0x1F;
    vn[5] = (CMD1>>15)&0x1F;

    vn[6] = (CMD0>>10)&0x1F;
    vn[7] = (CMD0>> 5)&0x1F;
    vn[8] = (CMD0    )&0x1F;

    vn[ 9] = (CMD0>>23)&0x1F;
    vn[10] = (CMD0>>18)&0x1F;
    vn[11] = (((CMD0>>13)&0x1c)|((CMD1>>30) & 0x3))&0x1F;

    DrawVisualTriangle(vn);
    DrawVisualTriangle(vn+3);
    DrawVisualTriangle(vn+6);
    DrawVisualTriangle(vn+9);

        //LOG_TO_FILE("%08X: %08X %08X CMD UC7_TRI2 (%i,%i,%i) (%i,%i,%i)\n", 
        //       ADDR, CMD0, CMD1, vn[0], vn[1], vn[2], vn[3], vn[4], vn[5]);

} // void rsp_uc07_tri2()


void rsp_uc07_quad()
{
    int     vn[6];

    DebugBox("12");

    vn[0] = ((rdp_reg.cmd0 >> 16) & 0xff) / 2;
    vn[1] = ((rdp_reg.cmd0 >>  8) & 0xff) / 2;
    vn[2] = ( rdp_reg.cmd0        & 0xff) / 2;

    vn[3] = ((rdp_reg.cmd1 >> 16) & 0xff) / 2;
    vn[4] = ((rdp_reg.cmd1 >>  8) & 0xff) / 2;
    vn[5] = ( rdp_reg.cmd1        & 0xff) / 2;

    DrawVisualTriangle(vn);
    DrawVisualTriangle(vn+3);

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_QUAD (%i,%i,%i) (%i,%i,%i)\n", 
                 ADDR, CMD0, CMD1, vn[0], vn[1], vn[2], vn[3], vn[4], vn[5]);
} // static void rsp_uc07_quad()



void rsp_uc07_fastcombine()
{
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_FAST_COMBINE",ADDR, CMD0, CMD1);
}

void rsp_uc07_rdphalf_cont()
{
//      bzaddr = segoffset2addr(rdp_reg.cmd1);
        LOG_TO_FILE("%08X: %08X %08X CMD UC7_RDP_HALF_1\n",ADDR, CMD0, CMD1);
}

void rsp_uc07_rdphalf_2()
{
        LOG_TO_FILE("%08X: %08X %08X CMD UC7_RDP_HALF_2\n",ADDR, CMD0, CMD1);
}

void rsp_uc07_line3d()
{
        LOG_TO_FILE("%08X: %08X %08X CMD UC7_LINE_3D\n",ADDR, CMD0, CMD1);
}

void rsp_uc07_texture()
{
        int idx;
        int tile = (rdp_reg.cmd0 >> 8)  & 0x07;             //** tile t0
        int tile1 = (tile + 1)  & 0x07;                     //** tile t1
        _u32 mipmap_level = (rdp_reg.cmd0 >> 11) & 0x07;    //** mipmap_level   - not used yet
        _u32 on = (rdp_reg.cmd0 & 0xff);            //** 1: on - 0:off

        float s = (float)((rdp_reg.cmd1 >> 16) & 0xffff);
        float t = (float)((rdp_reg.cmd1      ) & 0xffff);
        float SScale;
        float TScale;

        t_tile *tmp_tile = &rdp_reg.td[tile];
        t_tile *tmp_tile1 = &rdp_reg.td[tile1];
        tmp_tile->Texture_on = (_u8)on;

        rdp_reg.tile = tile;

        if (s<=1)
            SScale=1.0f;
        else
            SScale=(float)s/65535.f;

        if (t<=1)
            TScale=1.0f;
        else
            TScale=(float)t/65535.f;

        TScale/=32.f;
        SScale/=32.f;
        
        if( (((rdp_reg.cmd1)>>16)&0xFFFF) == 0xFFFF )
        {
            SScale = 1/32.0f;
        }
        else if( (((rdp_reg.cmd1)>>16)&0xFFFF) == 0x8000 )
        {
            SScale = 1/64.0f;
        }
        if( (((rdp_reg.cmd1)    )&0xFFFF) == 0xFFFF )
        {
            TScale = 1/32.0f;
        }
        else if( (((rdp_reg.cmd1)    )&0xFFFF) == 0x8000 )
        {
            TScale = 1/64.0f;
        }

        for (idx=0;idx<7;idx++)
        {
            rdp_reg.tile = idx;
            tmp_tile1 = &rdp_reg.td[idx];
            tmp_tile1->SScale=SScale;
            tmp_tile1->TScale=TScale;
            rdp_reg.m_CurTile = tmp_tile1;
            MathTextureScales();
        }

        rdp_reg.tile = tile;
        rdp_reg.m_CurTile = tmp_tile;
//      MathTextureScales();

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_TEXTURE",ADDR, CMD0, CMD1);
        LOG_TO_FILE("\tTile = %i, Mipmap = %i, enambled = %s\n",tile, mipmap_level, (on)?"on":"off");
}

void rsp_uc07_popmatrix()
{
//  _u32 param = (CMD1 / 64);
    _u32 param = (((((CMD0 >> 19) & 0x1f) + 1) * 8)/ 64);


    switch(CMD1)
    {
    case 0x40:
        pop_matrix();
        break;
    }

    LOG_TO_FILE("%08X: %08X %08X CMD UC7_POPMATRIX\n",ADDR, CMD0, CMD1);
}

void rsp_uc07_setgeometrymode()
{

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_SETGEOMETRYMODE",ADDR, CMD0, CMD1);
        LOG_TO_FILE(
                "\t+$%08lx:\n"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s"
                "%s",
                rdp_reg.cmd1,
                (rdp_reg.cmd1 & 0x00000001) ? "\t        zbuffer\n" : "",
                (rdp_reg.cmd1 & 0x00000002) ? "\t        texture\n" : "",
                (rdp_reg.cmd1 & 0x00000004) ? "\t        shade\n" : "",
                (rdp_reg.cmd1 & 0x00000200) ? "\t        shade smooth\n" : "",
                (rdp_reg.cmd1 & 0x00001000) ? "\t        cull front\n" : "",
                (rdp_reg.cmd1 & 0x00002000) ? "\t        cull back\n" : "",
                (rdp_reg.cmd1 & 0x00010000) ? "\t        fog\n" : "",
                (rdp_reg.cmd1 & 0x00020000) ? "\t        lightning\n" : "",
                (rdp_reg.cmd1 & 0x00040000) ? "\t        texture gen\n" : "",
                (rdp_reg.cmd1 & 0x00080000) ? "\t        texture gen lin\n" : "",
                (rdp_reg.cmd1 & 0x00100000) ? "\t        lod\n" : ""
                );

        rdp_reg.geometrymode &= rdp_reg.cmd0 & 0x0ffffff;
        rdp_reg.geometrymode |= rdp_reg.cmd1;

        if(rdp_reg.cmd1 & 0x00000002)
                rdp_reg.geometrymode_textures = 1;

        switch(rdp_reg.geometrymode & 0x00000600)
        {
        case 0x0200:
            Render_geometry_cullfront(1);
            break;
        case 0x0400:
            Render_geometry_cullback(1);
            break;
        case 0x0600:
            Render_geometry_cullfrontback(1);
            break;
        default:
            Render_geometry_cullfrontback(0);
            break;
        } //** switch(rdp_reg.geometrymode & 0x00003000) 


//      if((rdp_reg.geometrymode & 0x00020000) != 0)
//              rdp_reg.useLights = 1;

}

void rsp_uc07_matrix()
{

  static char *mtxop[] = {
                "modelview  mul  push",
                "modelview  mul  nopush",
                "modelview  load push",
                "modelview  load nopush",
                "projection mul  push",
                "projection mul  nopush",
                "projection load push",
                "projection load nopush"};

        _u32   a = segoffset2addr(rdp_reg.cmd1);
        _u8   command =(_u8)(CMD0 & 0xff);
        float   m[4][4];
//        int     i, j;


        hleGetMatrix((float*)&m[0,0], &pRDRAM[a]);

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_LOADMTX  at %08X\n", ADDR, CMD0, CMD1, a);
        LOG_TO_FILE(
                "\t%s\n"
                "\t\t{ %#+12.5f %#+12.5f %#+12.5f %#+12.5f }\n"
                "\t\t{ %#+12.5f %#+12.5f %#+12.5f %#+12.5f }\n"
                "\t\t{ %#+12.5f %#+12.5f %#+12.5f %#+12.5f }\n"
                "\t\t{ %#+12.5f %#+12.5f %#+12.5f %#+12.5f }\n",
                mtxop[command],
                m[0][0], m[0][1], m[0][2], m[0][3],
                m[1][0], m[1][1], m[1][2], m[1][3],
                m[2][0], m[2][1], m[2][2], m[2][3],
                m[3][0], m[3][1], m[3][2], m[3][3]
                );

        switch(command)
        {

            case 0: // modelview  mul  push   
                push_mult_matrix((GLfloat *)m);
                break;

            case 1: // modelview  mul  nopush 
                mult_matrix((GLfloat *)m);
                break;

            case 2: // modelview  load push   
                push_load_matrix((GLfloat *)m);
                break;

            case 3: // modelview  load nopush 
                load_matrix((GLfloat *)m);
                break;

            case 4: // projection mul  push   
                DebugBox("strange Matrix-CMD");
                //glMultMatrixf((GLfloat *)m);
                mult_prj_matrix((GLfloat *)m);
                break;

            case 5: // projection mul  nopush 
                //glMultMatrixf((GLfloat *)m);
                mult_prj_matrix((GLfloat *)m);
                break;

            case 6: // projection load push   
                DebugBox("strange Matrix-CMD");
                //glLoadMatrixf((GLfloat *)m);
                load_prj_matrix((GLfloat *)m);
                break;

            case 7: // projection load nopush 
                //glLoadMatrixf((GLfloat *)m);
                load_prj_matrix((GLfloat *)m);
                break;

            default:
//                PRINT_RDP_NOT_IMPLEMENTED("MATRIX: UNKNOWN COMMAND")
                ;

        } /* switch(command) */

}

void rsp_uc07_moveword()
{
        int i;
        _u16 offset= (_u16)(CMD0 & 0xffff);
        _u8 index=   (_u8)(CMD0>>16)&0xff;
        _u64 data=rdp_reg.cmd1;

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_MOVEWORD", ADDR, CMD0, CMD1);

        switch(index)
        {
            case G_MW_MATRIX:
                DebugBox("NI: MOVEWORD MATRIX\n");
                //RSP_RDP_InsertMatrix(gfx)
                break;

            case G_MW_NUMLIGHT:
                {
                    //rdp_reg.lights = (_u32)((data & 0xff) / 16 - 1);
                    int num_lights = (_u32)((data & 0x3ff) / 48);
                    if ((num_lights >= 0)&&(num_lights < 16))
                    {
                        rdp_reg.lights = num_lights;
                        rdp_reg.ambient_light =  num_lights;// + 1;
                    }
                }
                break;
        //uint32 dwNumLights = ((gfx->words.w1)/48);
        //gRSP.ambientLightIndex = dwNumLights+1;
        //SetNumLights(dwNumLights);

            case G_MW_CLIP:
                switch((rdp_reg.cmd0 >> 8) & 0xffff)
                {
                    case 0x0004:
                        rdp_reg.clip.nx = rdp_reg.cmd1;
                        LOG_TO_FILE("\tClip nx=%08i", CMD1);
                        break;

                    case 0x000c:
                        rdp_reg.clip.ny = rdp_reg.cmd1;
                        LOG_TO_FILE("\tClip ny=%08i", CMD1);
                        break;

                    case 0x0014:
                        rdp_reg.clip.px = rdp_reg.cmd1;
                        LOG_TO_FILE("\tClip px=%08i", CMD1);
                        break;

                    case 0x001c:
                        rdp_reg.clip.py = rdp_reg.cmd1;
                        LOG_TO_FILE("\tClip py=%08i", CMD1);
                        break;

                    default:
                        break;
                } /* switch((rdp_reg.cmd0 >> 8) & 0xffff) */               
                break;


            case G_MW_SEGMENT:
                LOG_TO_FILE("\tMOVEWORD SEGMENT: $%08lx -> seg#%d\n", CMD1, offset / 4);
                rdp_reg.segment[offset >> 2] = CMD1 & 0xffffff;
                break;

            case G_MW_FOG:
                {
                    float fo,fm,min,max,rng;
                    //(_SHIFTL(fm,16,16) | _SHIFTL(fo,0,16)))

                    //(_SHIFTL((128000/((max)-(min))),16,16) |
                    //_SHIFTL(((500-(min))*256/((max)-(min))),0,16)))

                    fm = (float)(_s16)((CMD1 & 0xffff0000)>> 16);
                    fo = (float)(_s16)((CMD1 & 0xffff));

                    rng = 128000.0f / fm;
                    min = 500.0f - ((fo * rng) / 256.0f);
                    max = rng + min;

                    rdp_reg.fog_fo = fo;
                    rdp_reg.fog_fm = fm;

                    LOG_TO_FILE("\tFog min = %f, max = %f",min,max);
                }
                break;

            case G_MW_LIGHTCOL:
                i = (rdp_reg.cmd0 & 0x0000e000) >> 13;
                i = offset / 0x18;
                LOG_TO_FILE("\tLight = %i, Color = %08X", i, CMD1);

                if(rdp_reg.cmd0 & 0x00000400)
                {
                        //PRINT_RDP("MOVEWORD LIGHTCOL ");
                        rdp_reg.light[i].r         = ((float)((rdp_reg.cmd1 >> 24) & 0xff))/255.0f;
                        rdp_reg.light[i].g         = ((float)((rdp_reg.cmd1 >> 16) & 0xff))/255.0f;
                        rdp_reg.light[i].b         = ((float)((rdp_reg.cmd1 >>  8) & 0xff))/255.0f;
                        rdp_reg.light[i].a         = 1.0f;

                }
                else
                {
                        //PRINT_RDP("MOVEWORD LIGHTCOL (copy) ");
                        rdp_reg.light[i].r_copy    = ((float)((rdp_reg.cmd1 >> 24) & 0xff))/255.0f;
                        rdp_reg.light[i].g_copy    = ((float)((rdp_reg.cmd1 >> 16) & 0xff))/255.0f;
                        rdp_reg.light[i].b_copy    = ((float)((rdp_reg.cmd1 >>  8) & 0xff))/255.0f;
                        rdp_reg.light[i].a_copy    = 1.0f;
                }

                //PRINT_RDP("%d: rgb?=$%08lx\n", i, rdp_reg.cmd1);
                //PRINT_RDP(
                //        "        rgba=%04.2f,%04.2f,%04.2f,%04.2f\n",
                //        ((float)((rdp_reg.cmd1 >> 24) & 0xff))/255.0f,
                //        ((float)((rdp_reg.cmd1 >> 16) & 0xff))/255.0f,
                //        ((float)((rdp_reg.cmd1 >>  8) & 0xff))/255.0f,
                //        1.0f
                //        );
                //calculate_light_vectors();
                break;

            case G_MW_FORCEMTX:
                DebugBox("G_MW_FORCEMTX\n");
                LOG_TO_FILE("\tG_MW_FORCEMTX\n");
                break;

            case G_MW_PERSPNORM:
                LOG_TO_FILE("\tG_MW_PERSPNORM\n");
                break;

            default: 
                //PRINT_RDP("MOVEWORD %x\n", index);
                ;

        } /* switch(rdp_reg.cmd0 & 0xff) */
}

void rsp_uc07_movemem()
{
        _u32 param;
//        int   i;

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_MOVEMEM",ADDR, CMD0, CMD1);

        param = (CMD0 & 0xff);
        switch(param)
        {
            case G_MV_MMTX:
                LOG_TO_FILE("\tMV_MMTX\n");
                DebugBox("MV_MMTX");
                break;

            case G_MV_PMTX:
                LOG_TO_FILE("\tMV_PMTX\n");
                //DebugBox("MV_PMTX");
                break;

            case G_MV_VIEWPORT:
                {
                    int a = segoffset2addr(rdp_reg.cmd1) & 0x007fffff;
                    a >>= 1;

                    rdp_reg.vp[0] = ((float)((_s16 *)pRDRAM)[(a+0)^1]) / 4.0f;
                    rdp_reg.vp[1] = ((float)((_s16 *)pRDRAM)[(a+1)^1]) / 4.0f;
                    rdp_reg.vp[2] = ((float)((_s16 *)pRDRAM)[(a+2)^1]) / 4.0f;
                    rdp_reg.vp[3] = ((float)((_s16 *)pRDRAM)[(a+3)^1]) / 4.0f;
                    rdp_reg.vp[4] = ((float)((_s16 *)pRDRAM)[(a+4)^1]) / 4.0f;
                    rdp_reg.vp[5] = ((float)((_s16 *)pRDRAM)[(a+5)^1]) / 4.0f;
                    rdp_reg.vp[6] = ((float)((_s16 *)pRDRAM)[(a+6)^1]) / 4.0f;
                    rdp_reg.vp[7] = ((float)((_s16 *)pRDRAM)[(a+7)^1]) / 4.0f;

                    Render_viewport();

                    LOG_TO_FILE("\tViewPort");
                    LOG_TO_FILE("\t{%f,%f,%f,%f",rdp_reg.vp[0],rdp_reg.vp[1],rdp_reg.vp[2],rdp_reg.vp[3]);
                    LOG_TO_FILE("\t %f,%f,%f,%f}\n",rdp_reg.vp[4],rdp_reg.vp[5],rdp_reg.vp[6],rdp_reg.vp[7]);
                }
                break;

            case G_MV_LIGHT:
                {
                    int a = segoffset2addr(rdp_reg.cmd1) & 0x007fffff;
                    int i = ((CMD0 >> 5) & 0x7ff);
                    if (i < G_MVO_L0)
                        break;
                    i = (i - G_MVO_L0) / 48; i--;
                    if (i >= 16)
                        break;
                    rdp_reg.light[i].r         = ((float)((_u8 *)pRDRAM)[(a+ 0)^3])/255.0f;
                    rdp_reg.light[i].g         = ((float)((_u8 *)pRDRAM)[(a+ 1)^3])/255.0f;
                    rdp_reg.light[i].b         = ((float)((_u8 *)pRDRAM)[(a+ 2)^3])/255.0f;
                    rdp_reg.light[i].a         = ((float)((_u8 *)pRDRAM)[(a+ 3)^3])/255.0f;
                    rdp_reg.light[i].r_copy    = ((float)((_u8 *)pRDRAM)[(a+ 4)^3])/255.0f;
                    rdp_reg.light[i].g_copy    = ((float)((_u8 *)pRDRAM)[(a+ 5)^3])/255.0f;
                    rdp_reg.light[i].b_copy    = ((float)((_u8 *)pRDRAM)[(a+ 6)^3])/255.0f;
                    rdp_reg.light[i].a_copy    = ((float)((_u8 *)pRDRAM)[(a+ 7)^3])/255.0f;
                    rdp_reg.light[i].x         = ((float)((_s8 *)pRDRAM)[(a+ 8)^3])/127.0f;
                    rdp_reg.light[i].y         = ((float)((_s8 *)pRDRAM)[(a+ 9)^3])/127.0f;
                    rdp_reg.light[i].z         = ((float)((_s8 *)pRDRAM)[(a+10)^3])/127.0f;
                    rdp_reg.light[i].w         = 1.0f;

                    LOG_TO_FILE("\tLight[%i]",i);
                    LOG_TO_FILE("\tRed = %f, Green = %f, Blue = %f, Alpha = %f",
                        rdp_reg.light[i].r,
                        rdp_reg.light[i].g,
                        rdp_reg.light[i].b,
                        rdp_reg.light[i].a);
                    LOG_TO_FILE("\tx = %f, y = %f, z = %f\n",
                        rdp_reg.light[i].x,
                        rdp_reg.light[i].y,
                        rdp_reg.light[i].z);
                }
                break;

            case G_MV_POINT:
                LOG_TO_FILE("\tG_MV_POINT\n");
                break;

            case G_MV_MATRIX: 
                
                dwConkerVtxZAddr = segoffset2addr(rdp_reg.cmd1) & 0x007fffff;

                LOG_TO_FILE("\tMV_MATRIX\n");
                DebugBox("MV_MATRIX");
                break;

            case G_MVO_LOOKATX:
                break;

            case G_MVO_LOOKATY:
                break;

            case G_MVO_L0:
            case G_MVO_L1:
            case G_MVO_L2:
            case G_MVO_L3:
            case G_MVO_L4:
            case G_MVO_L5:
            case G_MVO_L6:
            case G_MVO_L7:
                break;


            default:
                //DebugBox("unknow MoveMem %04x", param);
                break;

        } /* switch((rdp_reg.cmd0 >> 8) & 0xffff) */
}

void rsp_uc07_displaylist()
{
        _u32 addr = segoffset2addr(CMD1);
        _u8  push = (_u8)(CMD0 >> 16) & 0xff; 

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_DISPLAYLIST",ADDR, CMD0, CMD1);
        LOG_TO_FILE("\tAddress = %08x %s\n",addr, (push)?", Branch":", Push");

        switch(push)
        {
            case 0:   // push: we do a call of the dl 
                rdp_reg.pc_i++;
                if(rdp_reg.pc_i > RDP_STACK_SIZE-1)
                {
                    DebugBox("DList Stack overflow");
                    return;
                }
                rdp_reg.pc[rdp_reg.pc_i] = addr;
                break;

            case 1:   // branch 
                rdp_reg.pc[rdp_reg.pc_i] = addr;
                break;

            default:
                DebugBox("Unknow DList command");
                break;

        } // switch(push) 

}

void rsp_uc07_enddl()
{
        LOG_TO_FILE("%08X: %08X %08X CMD UC7_END_DISPLAYLIST\n",ADDR, CMD0, CMD1);

        if(rdp_reg.pc_i < 0)
        {
            DebugBox("EndDL - Display Stack underrun");
            rdp_reg.halt = 1;
            return;
        }

        if(rdp_reg.pc_i == 0)
        {
            rdp_reg.halt = 1;
        }

        rdp_reg.pc_i--;
}


void rsp_uc07_enddisplaylist()
{
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_END_DISPLAYLIST?\n",ADDR, CMD0, CMD1);
    rdp_reg.halt = 1;
    return;
}


void rsp_uc07_rdphalf_1()
{
    LOG_TO_FILE("%08X: %08X %08X CMD UC7_END_RDP_HALF_1\n",ADDR, CMD0, CMD1);
    bzaddr = segoffset2addr(rdp_reg.cmd1);
}

void rsp_uc07_setothermode_l()
{
        static char *ac[] = { "none", "threshold", "?", "diter" };
        static char *zs[] = { "pixel", "prim" };
        static char *a1[] =
                {
                        "        bl_1ma (1)",
                        "        bl_a_mem (1)",
                        "        bl_1 (1)",
                        "        bl_0 (1)"
                };
        static char *b1[] =
                {
                        "        bl_clr_in (1)",
                        "        bl_clr_mem (1)",
                        "        bl_clr_bl (1)",
                        "        bl_clr_fog (1)"
                };
        static char *c1[] =
                {
                        "        bl_a_in (1)",
                        "        bl_a_fog (1)",
                        "        bl_a_shade (1)",
                        "        bl_0 (1)"
                };
        static char *d1[] =
                {
                        "        bl_1ma (1)",
                        "        bl_a_mem (1)",
                        "        bl_1 (1)",
                        "        bl_0 (1)" 
                };
        static char *a2[] =
                {
                        "        bl_1ma (2)",
                        "        bl_a_mem (2)",
                        "        bl_1 (2)",
                        "        bl_0 (2)"
                };
        static char *b2[] =
                {
                        "        bl_clr_in (2)",
                        "        bl_clr_mem (2)",
                        "        bl_clr_bl (2)",
                        "        bl_clr_fog (2)"
                };
        static char *c2[] =
                {
                        "        bl_a_in (2)",
                        "        bl_a_fog (2)",
                        "        bl_a_shade (2)",
                        "        bl_0 (2)"
                };
        static char *d2[] =
                {
                        "        bl_1ma (2)",
                        "        bl_a_mem (2)",
                        "        bl_1 (2)",
                        "        bl_0 (2)" 
                };

        int len = ((rdp_reg.cmd0) & 0xff) + 1;
        int sft = 32 - ((rdp_reg.cmd0 >> 8) & 0xff) - len;

        switch (sft) //((rdp_reg.cmd0 >> 8) & 0xff)
        {
            case 0x00:
                //PRINT_RDP("SETOTHERMODE_L ALPHACOMPARE: ");
                //PRINT_RDP("%s\n", ac[(rdp_reg.cmd1>>0x00) & 0x3]);

                rdp_reg.mode_l &= ~0x00000003;
                rdp_reg.cmd1   &=  0x00000003;
                rdp_reg.mode_l |=  rdp_reg.cmd1;
                break;

            case 0x02:
                //PRINT_RDP("SETOTHERMODE_L ZSRCSEL: ");
                //PRINT_RDP("%s\n", zs[(rdp_reg.cmd1>>0x02) & 0x1]);

                rdp_reg.mode_l &= ~0x00000004;
                rdp_reg.cmd1   &=  0x00000004;
                rdp_reg.mode_l |=  rdp_reg.cmd1;
                break;

            case 0x03:
                //PRINT_RDP("SETOTHERMODE_L RENDERMODE: ");
                /*PRINT_RDP("$%08lx:\n", rdp_reg.cmd1 & 0xfffffff8,
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s"
                        "%s\n"
                        "%s\n"
                        "%s\n"
                        "%s\n"
                        "%s\n"
                        "%s\n"
                        "%s\n"
                        "%s\n",
                        rdp_reg.cmd1 & 0xfffffff8,
                        (rdp_reg.cmd1 & 0x00000008) ? "        anti alias\n" : "",
                        (rdp_reg.cmd1 & 0x00000010) ? "        z_cmp\n" : "",
                        (rdp_reg.cmd1 & 0x00000020) ? "        z_upd\n" : "",
                        (rdp_reg.cmd1 & 0x00000040) ? "        im_rd\n" : "",
                        (rdp_reg.cmd1 & 0x00000080) ? "        clr_on_cvg\n" : "",
                        (rdp_reg.cmd1 & 0x00000100) ? "        cvg_dst_warp\n" : "",
                        (rdp_reg.cmd1 & 0x00000200) ? "        cvg_dst_full\n" : "",
                        (rdp_reg.cmd1 & 0x00000400) ? "        z_inter\n" : "",
                        (rdp_reg.cmd1 & 0x00000800) ? "        z_xlu\n" : "",
                        (rdp_reg.cmd1 & 0x00001000) ? "        cvg_x_alpha\n" : "",
                        (rdp_reg.cmd1 & 0x00002000) ? "        alpha_cvg_sel\n" : "",
                        (rdp_reg.cmd1 & 0x00004000) ? "        force_bl\n" : "",
                        (rdp_reg.cmd1 & 0x00008000) ? "        tex_edge?\n" : "",
                        a2[(rdp_reg.cmd1>>16) & 0x3],
                        a1[(rdp_reg.cmd1>>18) & 0x3],
                        b2[(rdp_reg.cmd1>>20) & 0x3],
                        b1[(rdp_reg.cmd1>>22) & 0x3],
                        c2[(rdp_reg.cmd1>>24) & 0x3],
                        c1[(rdp_reg.cmd1>>26) & 0x3],
                        d2[(rdp_reg.cmd1>>28) & 0x3],
                        d1[(rdp_reg.cmd1>>30) & 0x3]
                        );*/
                rdp_reg.mode_l &= ~0xfffffff8;
                rdp_reg.cmd1   &=  0xfffffff8;
                rdp_reg.mode_l |=  rdp_reg.cmd1;
                break;

            case 0x10:
                rdp_reg.mode_l &= ~0xffff0000;
                rdp_reg.cmd1   &=  0xffff0000;
                rdp_reg.mode_l |=  rdp_reg.cmd1;
                //PRINT_RDP("SETOTHERMODE_L BLENDER\n");
                break;

            default:
                //PRINT_RDP("SETOTHERMODE_L ?\n");
                ;

        } /* switch((rdp_reg.cmd0 >> 8) & 0xff) */

        if(rdp_reg.mode_l & 0x00000010)
                Render_geometry_zbuffer(1);
        else
                Render_geometry_zbuffer(0);

        if ((rdp_reg.mode_l & 0x0c00) != 0xc00) glDepthRange(-1,1.);
        else  glDepthRange(-1,0.9995);

        if (rdp_reg.mode_l & 0x00000020)
            Render_geometry_zwrite(1);
        else
            Render_geometry_zwrite(0);
/*
    switch (rdp_reg.mode_l & 0x00000003)
    {
    case 0:
        Src_Alpha = GL_ONE;
        Dst_Alpha = GL_ZERO;
        break;
    case 1:
    case 2:
    case 3:
        Src_Alpha = GL_SRC_ALPHA;
        Dst_Alpha = GL_ONE_MINUS_SRC_ALPHA;
        break;
    }
*/

//  if ((rdp_reg.mode_l & 0x0f0f0000) != 0x50000)
    if ((rdp_reg.mode_l & 0x0f000000) != 0x0)
    {
        Src_Alpha = GL_SRC_ALPHA;
        Dst_Alpha = GL_ONE_MINUS_SRC_ALPHA;
    }
    else
    {
//      Src_Alpha = GL_ONE;
//      Dst_Alpha = GL_ZERO;
    }

}

void rsp_uc07_setothermode_h()
{
        static char *ad[] = { "pattern", "notpattern", "noise", "disable" };
        static char *rd[] = { "magicsq", "bayer", "noise", "?" };
        static char *ck[] = { "none", "key" };
        static char *tc[] = { "conv", "?", "?", "?", "?", "filtconv", "filt", "?" };
        static char *tf[] = { "point", "?", "bilerp", "average" };
        static char *tt[] = { "none", "?", "rgba16", "ia16" };
        static char *tl[] = { "tile", "lod" };
        static char *td[] = { "clamp", "sharpen", "detail", "?" };
        static char *tp[] = { "none", "persp" };
        static char *ct[] = { "1cycle", "2cycle", "copy", "fill" };
        static char *cd[] = { "disable(hw>1)", "enable(hw>1)", "disable(hw1)", "enable(hw1)" };
        static char *pm[] = { "nprimitive", "1primitive" };

        int len = (rdp_reg.cmd0 & 0xff) +1;
        int sft = 32 - ((rdp_reg.cmd0 >> 8) & 0xff) - len;
//        switch((rdp_reg.cmd0 >> 8) & 0xff)

        LOG_TO_FILE("%08X: %08X %08X CMD UC7_SETOTHERMODE_H\n",ADDR, CMD0, CMD1);
        switch(sft)
        {
            case 0x00:
                //PRINT_RDP("SETOTHERMODE_H BLENDMASK - ignored\n");
                break;

            case 0x04:
                //PRINT_RDP("SETOTHERMODE_H ALPHADITHER: ");
                //PRINT_RDP("%s\n", ad[(rdp_reg.cmd1>>0x04) & 0x3]);
                rdp_reg.mode_h &= ~0x00000030;
                rdp_reg.cmd1   &=  0x00000030;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x06:
                //PRINT_RDP("SETOTHERMODE_H RGBDITHER: ");
                //PRINT_RDP("%s\n", rd[(rdp_reg.cmd1>>0x06) & 0x3]);
                rdp_reg.mode_h &= ~0x000000c0;
                rdp_reg.cmd1   &=  0x000000c0;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x08:
                //PRINT_RDP("SETOTHERMODE_H COMBINEKEY: ");
                //PRINT_RDP("%s\n", ck[(rdp_reg.cmd1>>0x08) & 0x1]);
                rdp_reg.mode_h &= ~0x00000100;
                rdp_reg.cmd1   &=  0x00000100;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x09:
                //PRINT_RDP("SETOTHERMODE_H TEXTURECONVERT: ");
                //PRINT_RDP("%s\n", tc[(rdp_reg.cmd1>>0x09) & 0x7]);
                rdp_reg.mode_h &= ~0x00000e00;
                rdp_reg.cmd1   &=  0x00000e00;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x0c:
                //PRINT_RDP("SETOTHERMODE_H TEXTUREFILTER: ");
                //PRINT_RDP("%s\n", tf[(rdp_reg.cmd1>>0x0c) & 0x3]);
                rdp_reg.mode_h &= ~0x00003000;
                rdp_reg.cmd1   &=  0x00003000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x0e:
                //PRINT_RDP("SETOTHERMODE_H TEXTURELUT: ");
                //PRINT_RDP("%s\n", tt[(rdp_reg.cmd1>>0x0e) & 0x3]);
                rdp_reg.mode_h &= ~0x0000c000;
                rdp_reg.cmd1   &=  0x0000c000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x10:
                //PRINT_RDP("SETOTHERMODE_H TEXTURELOD: ");
                //PRINT_RDP("%s\n", tl[(rdp_reg.cmd1>>0x10) & 0x1]);
                rdp_reg.mode_h &= ~0x00010000;
                rdp_reg.cmd1   &=  0x00010000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x11:
                //PRINT_RDP("SETOTHERMODE_H TEXTUREDETAIL: ");
                //PRINT_RDP("%s\n", td[(rdp_reg.cmd1>>0x11) & 0x3]);
                rdp_reg.mode_h &= ~0x00060000;
                rdp_reg.cmd1   &=  0x00060000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x13:
                //PRINT_RDP("SETOTHERMODE_H TEXTUREPERSP: ");
                //PRINT_RDP("%s\n", tp[(rdp_reg.cmd1>>0x13) & 0x1]);
                rdp_reg.mode_h &= ~0x00080000;
                rdp_reg.cmd1   &=  0x00080000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x14:
                //PRINT_RDP("SETOTHERMODE_H CYCLETYPE: ");
                //PRINT_RDP("%s\n", ct[(rdp_reg.cmd1>>0x14) & 0x3]);
                cycle_mode = (_u8)((rdp_reg.cmd1>>0x14) & 0x3);
                rdp_reg.mode_h &= ~0x00300000;
                rdp_reg.cmd1   &=  0x00300000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x16:
                //PRINT_RDP("SETOTHERMODE_H COLORDITHER: ");
                //PRINT_RDP("%s\n", cd[(rdp_reg.cmd1>>0x16) & 0x1]);
                rdp_reg.mode_h &= ~0x00400000;
                rdp_reg.cmd1   &=  0x00400000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            case 0x17:
                //PRINT_RDP("SETOTHERMODE_H PIPELINEMODE: ");
                //PRINT_RDP("%s\n", pm[(rdp_reg.cmd1>>0x17) & 0x1]);
                rdp_reg.mode_h &= ~0x00800000;
                rdp_reg.cmd1   &=  0x00800000;
                rdp_reg.mode_h |=  rdp_reg.cmd1;
                break;

            default:
                //PRINT_RDP("SETOTHERMODE_H\n");
                ;

        } /* switch((rdp_reg.cmd0 >> 8) & 0xff) */


}



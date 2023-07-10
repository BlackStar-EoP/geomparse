// geomparse.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <assert.h>
#include <sstream>
#include <regex>
#include "half.hpp"

inline uint32_t Reverse32(uint32_t value)
{
    return (((value & 0x000000FF) << 24) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0xFF000000) >> 24));
}

inline uint16_t Reverse16(uint16_t value)
{
    return (((value & 0x00FF) << 8) |
             ((value & 0xFF00) >> 8));
}

float ReverseFloat(const float inFloat)
{
    float retVal;
    char* floatToConvert = (char*)&inFloat;
    char* returnFloat = (char*)&retVal;

    // swap the bytes into a temporary buffer
    returnFloat[0] = floatToConvert[3];
    returnFloat[1] = floatToConvert[2];
    returnFloat[2] = floatToConvert[1];
    returnFloat[3] = floatToConvert[0];

    return retVal;
}

uint32_t parse32(uint8_t* data, uint32_t& offset)
{
    uint32_t* buf = (uint32_t*)&data[offset];
    offset += sizeof(uint32_t);
    return Reverse32(*buf);
}

uint16_t parse16(uint8_t* data, uint32_t& offset)
{
    uint16_t* buf = (uint16_t*)&data[offset];
    offset += sizeof(uint16_t);
    return Reverse16(*buf);
}

uint8_t parse8(uint8_t* data, uint32_t& offset)
{
    uint8_t val = data[offset];
    ++offset;
    return val;
}

float parsef32(uint8_t* data, uint32_t& offset)
{
    uint32_t be = parse32(data, offset);
    float f;
    std::memcpy(&f, &be, sizeof(float));
    return f;
}

float parsef16(uint8_t* data, uint32_t& offset)
{
    uint32_t be = parse16(data, offset);

    uint32_t floatval = (((uint32_t)be & 0x8000) << 16) | (((uint32_t)be & 0x7FFF) << 13) + 0x38000000;
    float fv;
    std::memcpy(&fv, &floatval, sizeof(float));

    half_float::half f;
    std::memcpy(&f.data_, &be, sizeof(uint16_t));
    float ff = float(f);
    return ff;
}

//void parse_4_bytes(uint8_t* data)
//{
//    if (offset == 0x1C4)
//        printf("");
//    uint32_t* buf = (uint32_t*)&data[offset];
//    uint32_t le = *buf;
//    uint32_t be = Reverse32(le);
//
//    float befp = (float)be;
//    float lefp = (float)le;
//    float befp2 = ReverseFloat(lefp);
//    float befp3;
//    std::memcpy(&befp3, &be, sizeof(float));
//
//    printf("Offset: 0x%x, hex = 0x%08x value = %f\n", offset, le, befp3);
//
//    offset += 4;
//}

/*uint32_t version; uint32_t texcount; struct { char name[64]; char texfile[64]; } textures[texcount]; */

struct GeomTexture
{
    std::string name;
    std::string texfile;
};

struct GeomMaterialEntry
{
    uint32_t texcount;
    std::vector<GeomTexture> textures;
    uint32_t id;

    std::string name()
    {
        return textures[0].name;
    }

    std::string getfilename()
    {
        std::stringstream ss;
        assert(textures.size() > 0);
        ss << id << "_" << textures[0].name << ".mtl";
        return ss.str();
    }

    void dumpMaterial()
    {
        /*
        newmtl Wood
            Ka 1.000000 1.000000 1.000000
            Kd 0.640000 0.640000 0.640000
            Ks 0.500000 0.500000 0.500000
            Ns 96.078431
            Ni 1.000000
            d 1.000000
            illum 0
            map_Kd woodtexture.jpg

            The example uses the following keywords :

    Ka: specifies ambient color, to account for light that is scattered about the entire scene[see Wikipedia entry for Phong Reflection Model] using values between 0 and 1 for the RGB components.
        Kd : specifies diffuse color, which typically contributes most of the color to an object[see Wikipedia entry for Diffuse Reflection].In this example, Kd represents a grey color, which will get modified by a colored texture map specified in the map_Kd statement
        Ks : specifies specular color, the color seen where the surface is shinyand mirror - like[see Wikipedia entry for Specular Reflection].
        Ns : defines the focus of specular highlights in the material.Ns values normally range from 0 to 1000, with a high value resulting in a tight, concentrated highlight.
        Ni : defines the optical density(aka index of refraction) in the current material.The values can range from 0.001 to 10. A value of 1.0 means that light does not bend as it passes through an object.
        d : specifies a factor for dissolve, how much this material dissolves into the background.A factor of 1.0 is fully opaque.A factor of 0.0 is completely transparent.
        illum : specifies an illumination model, using a numeric value.See Notes below for more detail on the illum keyword.The value 0 represents the simplest illumination model, relying on the Kd for the material modified by a texture map specified in a map_Kd statement if present.The compilers of this resource believe that the choice of illumination model is irrelevant for 3D printing use and is ignored on import by some software applications.For example, the MTL Loader in the threejs Javascript library appears to ignore illum statements.Comments welcome.
        map_Kd : specifies a color texture file to be applied to the diffuse reflectivity of the material.During rendering, map_Kd values are multiplied by the Kd values to derive the RGB components.
*/
        std::string filename = getfilename();
        FILE* fp = fopen(filename.c_str(), "w+");
        fprintf(fp, "newmtl %s\n", textures[0].name.c_str());
        fprintf(fp, "Ka 1.000000 1.000000 1.000000\n");
        fprintf(fp, "Kd 1.000000 1.000000 1.000000\n");
        fprintf(fp, "Ks 0.000000 0.000000 0.000000\n");
        fprintf(fp, "map_Kd %s\n", textures[0].texfile.c_str());
        fclose(fp);
    }
};

std::string parseString(uint8_t* data, uint32_t offset)
{
    char str[64];
    memset(str, 0, sizeof(str));
    memcpy(str, data + offset, 64);
    offset += 64;
    str[63] = 0;
    return std::string(str);
}
struct GeomMaterial
{
    uint32_t num_materials;
    std::vector<GeomMaterialEntry> materialEntries;
    
    void parse(uint8_t* data)
    {
        uint32_t offset = 0u;
        num_materials = parse32(data, offset);
        for (uint32_t m = 0; m < num_materials; ++m)
        {
            GeomMaterialEntry e;
            e.texcount = parse32(data, offset);
            e.id = m;
            for (uint32_t tex = 0; tex < e.texcount; ++tex)
            {
                GeomTexture t;
                t.name = parseString(data, offset);
                t.texfile = parseString(data, offset);
                e.textures.push_back(t);
            }
            materialEntries.push_back(e);
        }
    }

    void dumpMaterials()
    {
        for (auto& e : materialEntries)
        {
            e.dumpMaterial();
        }
    }

};

GeomMaterial geommaterial;

struct GeomHeader
{
    uint32_t num_meshes;
    uint32_t unk1;
    uint32_t unk2;
    uint32_t filesize;
};

struct GeomAABB
{
    float minX;
    float minY;
    float minZ;
    float maxX;
    float maxY;
    float maxZ;
};

struct GeomMeshHeader
{
    uint32_t signature;
    uint16_t unk1;
    uint8_t unk2;
    uint8_t materialId;
    uint16_t unk3;
    uint16_t unk4;
    uint32_t allFF;

    uint32_t meshTrianglesAddress;
    uint16_t meshTrianglesSize;
    uint8_t* triangle_data;

    uint16_t padding1;

    uint32_t meshBlock1Address;
    uint32_t meshBlock1EndAddress;
    uint16_t meshBlock1Length;
    uint16_t padding2;

    uint32_t unk32_1;
    uint32_t unk32_2;

    uint32_t textureBlock1Address;
    uint32_t textureBlock1Length;

    static const uint32_t NUM_OFFSETS = 19;

    uint32_t offsets[NUM_OFFSETS];

    uint32_t num_vertices;
    uint32_t num_tex_coords;

    GeomAABB aabb_;

    struct MeshFloat
    {
        float m_val;
        uint32_t m_addr;
        MeshFloat(uint32_t addr, float val)
        {
            m_val = val;
            m_addr = addr;
        }
    };


    struct MeshVertex
    {
        float vx, vy, vz;
        float tx, ty;
        uint32_t id_;
        MeshVertex(uint32_t id, float x, float y, float z, GeomAABB& aabb)
            : id_(id), vx(x), vy(y), vz(z)
        {
            if ((vx < aabb.minX || vx > aabb.maxX))
                isValid = false;
            if ((vy < aabb.minY || vy > aabb.maxY))
                isValid = false;
            if ((vz < aabb.minZ || vz > aabb.maxZ))
                isValid = false;
        }

        const float EPSILON = 0.00001f;

        bool operator == (const MeshVertex& v) const
        {
            return (abs(vx - v.vx) < EPSILON) &&
                   (abs(vy - v.vy) < EPSILON) &&
                   (abs(vz - v.vz) < EPSILON);
        }

        bool isValid = true;
        std::vector<uint32_t> duplicates;
    };

    std::vector<MeshVertex> meshBlock1;

    void parse(GeomAABB& aabb, uint8_t* data, uint32_t offset)
    {
        aabb_ = aabb;
        signature = parse32(data, offset);
        unk1 = parse16(data, offset);
        unk2 = parse8(data, offset);
        materialId = parse8(data, offset);
        unk3 = parse16(data, offset);
        unk4 = parse16(data, offset);
        allFF = parse32(data, offset);

        meshTrianglesAddress = parse32(data, offset);
        meshTrianglesSize = parse16(data, offset);
        padding1 = parse16(data, offset);

        meshBlock1Address = parse32(data, offset);
        meshBlock1EndAddress = parse32(data, offset);
        meshBlock1Length = parse16(data, offset);
        
        num_vertices = meshBlock1Length / 4 / 3;

        padding2 = parse16(data, offset);

        unk32_1 = parse32(data, offset);
        unk32_2 = parse32(data, offset);

        textureBlock1Address = parse32(data, offset);
        textureBlock1Length = parse32(data, offset);
        num_tex_coords = textureBlock1Length / 2 / 2;

        for (uint32_t i = 0; i < NUM_OFFSETS; ++i)
        {
            offsets[i] = parse32(data, offset);
        }
    }

    void readTriangleData(uint8_t* data)
    {
        triangle_data = new uint8_t[meshTrianglesSize];
        memcpy(triangle_data, data + meshTrianglesAddress, meshTrianglesSize);
        uint32_t offset = 0;

        uint16_t val1 = parse16(triangle_data, offset);
        uint16_t val2 = parse16(triangle_data, offset);
        uint16_t val3 = parse16(triangle_data, offset);
        uint16_t val4 = parse16(triangle_data, offset);
        uint16_t val5 = parse16(triangle_data, offset);
        uint16_t val6 = parse16(triangle_data, offset);
        uint16_t val7 = parse16(triangle_data, offset);
        uint16_t val8 = parse16(triangle_data, offset);

        
        printf("");
    }

    void parseBlock1(uint8_t* data)
    {
        uint32_t offset = meshBlock1Address;
        uint32_t length = meshBlock1EndAddress - meshBlock1Address;
        assert(length == meshBlock1Length);
        uint32_t id = 0;
        for (uint16_t v = 0; v < meshBlock1Length / 4; v += 3)
        {
            float x = parsef32(data, offset);
            float y = parsef32(data, offset);
            float z = parsef32(data, offset);
            meshBlock1.push_back(MeshVertex(id++, x, y, z, aabb_));
        }

        offset = textureBlock1Address;
        

        for (uint16_t t = 0; t < (textureBlock1Length / 2 / 2); t++)
        {
            float t1 = parsef16(data, offset);
            float t2 = parsef16(data, offset);
            meshBlock1[t].tx = t1;
            meshBlock1[t].ty = t2;
        }

        uint32_t v = 0;
        while (v < meshBlock1.size())
        {
            MeshVertex& vertex = meshBlock1[v];
            ++v;
            for (uint32_t dup = v; dup < meshBlock1.size(); ++dup)
            {
                MeshVertex& vertex2 = meshBlock1[dup];
                if (vertex == vertex2)
                {
                    vertex.duplicates.push_back(vertex2.id_);
                    vertex2.duplicates.push_back(vertex.id_);
                }
            }
            
        }
    }


    void dumpBlock1ToOBJ(const std::string& filename)
    {
        FILE* dmp = fopen(filename.c_str(), "w+");
        
        if (dmp)
        {
            GeomMaterialEntry mat = geommaterial.materialEntries[materialId];

            fprintf(dmp, "mtllib %s\n", mat.getfilename().c_str());
            fprintf(dmp, "usemtl %s\n", mat.name().c_str());
            /*
            usemtl     Material name : usemtl materialname
                mtllib     Material library : mtllib materiallibname.mtl
                */
         //   mat.
            uint32_t index = 1;
            for (uint32_t v = 0; v < meshBlock1.size(); ++v)
            {
                MeshVertex& vertex = meshBlock1[v];
                //fprintf(dmp, "# addr = %08x\n", meshBlock1[v].m_addr);
                fprintf(dmp, "#%u ", v + 1);


                if (vertex.duplicates.size() > 0)
                {
                    fprintf(dmp, "duplicate of (");
                    for (uint32_t dup : vertex.duplicates)
                    {
                        fprintf(dmp, "%u, ", dup + 1);
                    }
                   fprintf(dmp, ")\n");
                }

                if (!vertex.isValid)
                {
                    fprintf(dmp, "INVALID, outside AABB\n");
                }

                fprintf(dmp, "v %f %f %f\n", vertex.vx, vertex.vy, vertex.vz);
                fprintf(dmp, "vt %f %f\n\n", vertex.tx, vertex.ty * -1);
                //fprintf(dmp, "f -3/-3 -2/-2 -1/-1\n");
                //if (index % 4 == 0)
                //{
                //    fprintf(dmp, "f -4/-4 -3/-3 -2/-2 -1/-1\n");
                //}
                index++;
            }

            uint32_t vertex = 3;
            while (vertex <= meshBlock1.size())
            {
                fprintf(dmp, "f %u/%u %u/%u %u/%u\n", vertex-2, vertex - 2, vertex - 1, vertex - 1, vertex, vertex);
                vertex+=3;
            }

            fclose(dmp);
        }

    }
};

struct Geom
{
    Geom(const std::string& filename, uint32_t filesize)
    {
        m_filename = filename;
        m_filesize = filesize;
    }

    GeomHeader geomheader;
    std::vector<GeomMeshHeader> meshHeaders;

    uint32_t offset = 0u;
    void parse(uint8_t* data)
    {
        
        geomheader.num_meshes = parse32(data, offset);
        geomheader.unk1 = parse32(data, offset);
        geomheader.unk2 = parse32(data, offset);
        geomheader.filesize = parse32(data, offset);

        uint32_t aabboffset = m_filesize - (6 * sizeof(float));
        aabb.maxX = parsef32(data, aabboffset);
        aabb.maxY = parsef32(data, aabboffset);
        aabb.maxZ = parsef32(data, aabboffset);
        aabb.minX = parsef32(data, aabboffset);
        aabb.minY = parsef32(data, aabboffset);
        aabb.minZ = parsef32(data, aabboffset);
    }

    void parseMeshHeaders(uint8_t* data)
    {
        for (uint32_t i = 0; i < geomheader.num_meshes; ++i)
        {
            GeomMeshHeader h;
            h.parse(aabb, data, offset);
            meshHeaders.push_back(h);
        }
    }

    void parseMesh(uint8_t* data)
    {
        for (int i = 0; i < meshHeaders.size(); ++i)
        //for (auto& mesh : meshHeaders)
        {
            meshHeaders[i].parseBlock1(data);
            meshHeaders[i].readTriangleData(data);
            //mesh.parseBlock1(data);
        }
    }

    void dump_meshes()
    {
        for (size_t i = 0; i < meshHeaders.size(); ++i)
        {
            std::stringstream str;
            str << m_filename << i << ".obj";
            meshHeaders[i].dumpBlock1ToOBJ(str.str());
        }
        
    }

    std::string m_filename;
    uint32_t m_filesize;
    GeomAABB aabb;
};

int main(int argc, char* argv[])
{
#ifndef _DEBUG
    if (argc != 2)
        return -1;
    const char* file = argv[1];
#else
    const char* file = "D:/trash panic/reveng/Stage1_Geom.dmp/BaboCoin/BaboCoin_MASTER.geom.edge";
    // 
    //const char* file = "D:/trash panic/reveng/Stage1_Geom.dmp/Superball/superball_MASTER.geom.edge";
    //const char* file = "D:/trash panic/reveng/Stage1_Geom.dmp/RES_MDL_S_STAGE/gomibako_gomibako_1.geom.edge";
    //const char* file  = "D:/trash panic/reveng/Stage1_Geom.dmp/Piggybank/piggybank_MASTER.geom.edge";
      //const char* file = "D:/trash panic/reveng/Stage1_Geom.dmp/YUDEN/YUDEN_MASTER.geom.edge";
    //const char* file = "D:/trash panic/reveng/Stage2_Geom.dmp/LCTV/LCTV_MASTER.geom.edge";
#endif    
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0L, SEEK_END);
    int geomsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    uint8_t* data = new uint8_t[geomsize];
    fread(data, 1, geomsize, fp);
    fclose(fp);

    std::string material = std::regex_replace(file, std::regex("geom.edge"), "mat.edge");
    fp = fopen(material.c_str(), "rb");
    fseek(fp, 0L, SEEK_END);
    int matsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    uint8_t* matdata = new uint8_t[matsize];
    fread(matdata, 1, matsize, fp);
    fclose(fp);

    GeomMaterial m;
    m.parse(matdata);
    m.dumpMaterials();
    geommaterial = m;

    Geom g(file, geomsize);
    g.parse(data);
    g.parseMeshHeaders(data);
    g.parseMesh(data);
    g.dump_meshes();

    //while (offset < size)
    //{
    //    parse_4_bytes(data);
    //}

}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

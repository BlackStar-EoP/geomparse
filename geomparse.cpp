// geomparse.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <assert.h>
#include <sstream>
#include <regex>
#include "half.hpp"

uint8_t* readfile(const std::string& file, int& size)
{
    FILE* fp = fopen(file.c_str(), "rb");
    if (fp)
    {
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        uint8_t* data = new uint8_t[size];
        fread(data, 1, size, fp);
        fclose(fp);
        return data;
    }

    size = 0;
    return nullptr;
}

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

    void dumpMaterial(const std::string& path)
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
        std::string filename = path + getfilename();
        FILE* fp = fopen(filename.c_str(), "w+");
        fprintf(fp, "newmtl %s\n", textures[0].name.c_str());
        fprintf(fp, "Ka 1.000000 1.000000 1.000000\n");
        fprintf(fp, "Kd 1.000000 1.000000 1.000000\n");
        fprintf(fp, "Ks 0.000000 0.000000 0.000000\n");
        fprintf(fp, "map_Kd %s\n", textures[0].texfile.c_str());
        fclose(fp);
    }
};

std::string parseString(uint8_t* data, uint32_t& offset)
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

    void dumpMaterials(const std::string& path)
    {
        for (auto& e : materialEntries)
        {
            e.dumpMaterial(path);
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

struct MeshTriangle
{
    uint32_t v1_, v2_, v3_;
    uint8_t nibble;
    MeshTriangle(uint32_t v1, uint32_t v2, uint32_t v3, uint8_t nibbleval)
        : v1_(v1), v2_(v2), v3_(v3), nibble(nibbleval)
    {}

    std::string v1()
    {
        std::stringstream ss;
        ss << (v1_ + 1) << "/" << (v1_ + 1);
        return ss.str();
    }
    std::string v2()
    {
        std::stringstream ss;
        ss << (v2_ + 1) << "/" << (v2_ + 1);
        return ss.str();
    }
    std::string v3()
    {
        std::stringstream ss;
        ss << (v3_ + 1) << "/" << (v3_ + 1);
        return ss.str();
    }

    bool operator == (const MeshTriangle& t2) const
    {
        return (v1_ == t2.v1_) &&
               (v2_ == t2.v2_) &&
               (v3_ == t2.v3_);
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
        float xd = abs(vx - v.vx);
        float yd = abs(vy - v.vy);
        float zd = abs(vz - v.vz);
        return (xd < EPSILON) &&
               (yd < EPSILON) &&
               (zd < EPSILON);
    }

    bool isValid = true;
    std::vector<uint32_t> duplicates;
};



std::vector<MeshVertex> parsedVertices;


void parse_vertex_array(uint8_t* vtxdata, uint32_t vtxsize)
{
    uint32_t offset = 0x6F0;
    uint32_t id = 0u;
    GeomAABB aabb;
    while (offset < vtxsize)
    {
        float x = parsef32(vtxdata, offset);
        float y = parsef32(vtxdata, offset);
        float z = parsef32(vtxdata, offset);
        float u1 = parsef32(vtxdata, offset);
        float u2 = parsef32(vtxdata, offset);

        parsedVertices.push_back(MeshVertex(id++, x, y, z, aabb));
        //uint16_t i1 = parse16(idxdata, offset);
        //uint16_t i2 = parse16(idxdata, offset);
        //uint16_t i3 = parse16(idxdata, offset);
        //parsedTriangles.push_back(MeshTriangle(i1, i2, i3, 0));
    }
    printf("");
}


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
    uint8_t* m_triangle_data;

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

    std::vector<float> floatsblock1;
    std::vector<float> floatsblock2;

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

    std::vector<MeshVertex> meshBlock1;
    std::vector<MeshTriangle> triangles;
    std::vector<MeshTriangle> parsedTriangles;

    void parse(GeomAABB& aabb, uint8_t* data, uint32_t& offset)
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

    void parsenib2(uint8_t nib, uint8_t prevnib, uint32_t& v)
    {
        switch (nib)
        {
        case 0x00:
            break;
        case 0x01:
            triangles.push_back(MeshTriangle(v - 1, v, v + 1, nib));
            triangles.push_back(MeshTriangle(v + 1, v, v + 2, nib));
            v += 2;
            break;
        case 0x02:
            break;
        case 0x03:
            printf("");
            break;
        case 0x04:
            triangles.push_back(MeshTriangle(v - 1, v - 3, v - 4, nib));
            triangles.push_back(MeshTriangle(v - 1, v - 4, v, nib));
            v += 2;
            printf("");
            break;
        case 0x05:
            triangles.push_back(MeshTriangle(v, v - 2, v + 1, nib));
            triangles.push_back(MeshTriangle(v + 1, v - 2, v + 2, nib));
            v += 3;
            break;
        case 0x06:
            break;
        case 0x07:
            triangles.push_back(MeshTriangle(v - 1, v - 2, v, nib));
            triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
            v += 4;
            break;

        case 0x08:
            break;
        case 0x09:
            break;
        case 0x0A:
            break;
        case 0x0B:
            break;
        case 0x0C:
            break;
        case 0x0D:
            /* confirmed from the working index arrays, 1 quad from 4 vertices */
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            triangles.push_back(MeshTriangle(v + 2, v + 1, v + 3, nib));
            v += 4;

            break;
        case 0x0E:
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            triangles.push_back(MeshTriangle(v + 1, v, v + 3, nib));
            v += 3;
            break;

        case 0x0F:
            /* confirmed from the working index arrays, 2 hard triangles */
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            v += 3;
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            v += 3;
            break;
        }

        if (triangles.size() <= parsedTriangles.size() && triangles.size() > 1)
        {
            int idx1 = triangles.size() - 2;
            int idx2 = triangles.size() - 1;

            if (triangles[idx1] == parsedTriangles[idx1])
                printf("");
            else
                printf("");

            if (triangles[idx2] == parsedTriangles[idx2])
                printf("");
            else
                printf("");
        }

    }

    
    void parsenib(uint8_t nib, uint8_t prevnib, uint32_t& v)
    {
        switch (nib)
        {
        case 0x00:
            v += 6;
            break;
        case 0x01:
            if (prevnib != 0x0E)
            {
                /* MONOLITH_L_MASTER works prev nib is 0x0E */
                v -= 1;
                triangles.push_back(MeshTriangle(v, v + 2, v + 4, nib));
                triangles.push_back(MeshTriangle(v, v + 4, v + 3, nib));
                v += 5;
            }
            if (prevnib == 0x0D)
            {
                //* MONOLITH_L break 1 */
                v -= 6;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 5, nib));
                v += 6;
            }
            else
            {
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
            }
            break;

        case 0x02:
            break;
        case 0x03:
            if (prevnib == 0x0F)
            {
                // works in MONOLITH_BIG_damage_Mesh.geom.edge0
                v -= 1;
                triangles.push_back(MeshTriangle(v + 2, v, v + 1, nib));
                v += 1;

                triangles.push_back(MeshTriangle(v, v + 3, v + 1, nib));
                v += 4;
            }
            if (prevnib == 0x0D)
            {
                v -= 2;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 3;

                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                //v += 4;

            }
            break;
        
        case 0x04:
            if (prevnib == 0x07)
            {
                v -= 2;

                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;

                v += 2;

            }
            else
            {
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
            }
            break;

        case 0x05:

            if (prevnib == 0x01) /* monolith L break 1 */
            {
                v -= 4;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v, v + 2, v + 3, nib));
                v += 4;

            }
            if (prevnib == 0x0E) /* monolith L break 4 */
            {
            }
            if (prevnib == 0x0D)
            {
                // works for monolith L damage 3
                v -= 1;

                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v, v + 2, v + 5, nib));
                v += 3;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
            }
            break;

        case 0x06:
            break;
        case 0x07:
            if (prevnib == 0x0F)
            {
                // MONOLITH_LG_break_break_3 seems correct
                v -= 2; // TODO confirm
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
                v += 2;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
                v += 4;
            }
            if (prevnib == 0x0D)
            {
                v -= 3;
                triangles.push_back(MeshTriangle(v, v + 2, v + 3, nib));
                v += 3;
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
                v += 4;
            }
            if (prevnib == 0x03)
            {
                v -= 1;
                triangles.push_back(MeshTriangle(v, v + 2, v + 4, nib));
                v += 4;
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));

                v += 3;
            }
            if (prevnib == 0x07)
            {
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                v += 1;

                v += 1;
            }

            
            break;
        case 0x08:
            break;
        case 0x09:
            break;
        case 0x0A:
            break;
        case 0x0B:
            break;
        
        case 0x0C:
            // Works in MONOLITH_BOX_break_break_1.geom.edge0
            triangles.push_back(MeshTriangle(v, v + 2, v + 1, nib));
            triangles.push_back(MeshTriangle(v, v + 3, v + 2, nib));
            v += 4;
            break;

        case 0x0D:
            if (prevnib != 5)
            {
                // works for most
                triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
            }
            else
            {
                // works for monolith l damage 3
                triangles.push_back(MeshTriangle(v, v + 1, v + 3, nib));
                triangles.push_back(MeshTriangle(v + 1, v + 2, v + 3, nib));
            }
            v += 4;
            break;

        case 0x0E:
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            //v += 1;
            triangles.push_back(MeshTriangle(v, v + 1, v + 3, nib));
            //v += 1;
            v += 2;
            break;

        case 0x0F:
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            v += 3;
            triangles.push_back(MeshTriangle(v, v + 1, v + 2, nib));
            v += 3;
            break;
        }
    }

    void writefaces(std::vector<uint8_t>& prefacedata, std::vector<uint8_t>& facedata)
    {
        uint32_t v = 0u;
        uint8_t prevnib = 0u;
        for (uint8_t face : facedata)
        {
            uint8_t nib1 = (face >> 4) & 0x0F;
            uint8_t nib2 = face & 0x0F;
            parsenib2(nib1, prevnib, v);
            prevnib = nib1;
            parsenib2(nib2, prevnib, v);
            prevnib = nib2;
        }
    }

    void readTriangleData(uint8_t* data)
    {
        m_triangle_data = new uint8_t[meshTrianglesSize];
        memcpy(m_triangle_data, data + meshTrianglesAddress, meshTrianglesSize);
        uint32_t offset = 0;

        uint16_t val1 = parse16(m_triangle_data, offset);
        uint16_t val2 = parse16(m_triangle_data, offset);
        uint16_t facedatastart = parse16(m_triangle_data, offset);
        uint16_t val4 = parse16(m_triangle_data, offset);

        std::vector<uint8_t> prefacedata;
        std::vector<uint8_t> facedata;
        for (uint16_t i = 0; i < facedatastart; ++i)
        {
            prefacedata.push_back(m_triangle_data[i + 8]);
        }

        for (uint16_t i = 8 + facedatastart; i < meshTrianglesSize; ++i)
        {
            facedata.push_back(m_triangle_data[i]);
        }

        writefaces(prefacedata, facedata);
    }

    void readTriangleDataFromIndexArray(const std::string& filename, int number)
    {

        std::stringstream ss;
        ss << filename << ".idx." << number;

        int idxsize;
        uint8_t* idxdata = readfile(ss.str(), idxsize);
        if (idxdata != nullptr)
        {
            uint32_t offset = 0;
            while (offset < idxsize)
            {
                uint16_t i1 = parse16(idxdata, offset);
                uint16_t i2 = parse16(idxdata, offset);
                uint16_t i3 = parse16(idxdata, offset);
                parsedTriangles.push_back(MeshTriangle(i1, i2, i3, 0));
            }
        }
    }

    void parseFloatBlock(uint8_t* data)
    {
        uint32_t offset = meshBlock1EndAddress;
        while (offset < textureBlock1Address)
        {
            float val = parse32(data, offset);
            floatsblock1.push_back(val);
        }

        offset = meshBlock1EndAddress;
        while (offset < textureBlock1Address)
        {
            float val = parse16(data, offset);
            floatsblock2.push_back(val);
        }

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
        
        int min = meshBlock1.size() > parsedVertices.size() ? meshBlock1.size() : parsedVertices.size();
        //for (int i = 0; i < min; ++i)
        //{
        //    if (parsedVertices[i] == meshBlock1[1])
        //    {
        //    }
        //    else
        //    {
        //        printf("");
        //    }
        //}



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
            //for (uint32_t v = 0; v < parsedVertices.size(); ++v)
            {
                MeshVertex& vertex = meshBlock1[v];
                //MeshVertex& vertex = parsedVertices[v];
                //fprintf(dmp, "# addr = %08x\n", meshBlock1[v].m_addr);
                fprintf(dmp, "#%u ", v + 1);


                if (vertex.duplicates.size() > 0)
                {
                    fprintf(dmp, "duplicate of (");
                    for (uint32_t dup : vertex.duplicates)
                    {
                        fprintf(dmp, "%u, ", dup + 1);
                    }
                   fprintf(dmp, ")");
                }
                fprintf(dmp, "\n");

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

            for (uint16_t i = 0; i < meshTrianglesSize; ++i)
            {
                if ((i % 16 == 0) && (i > 0))
                {
                    fprintf(dmp, "\n");
                }

                uint8_t val = m_triangle_data[i];
                fprintf(dmp, "%02X ", val);

            }

            fprintf(dmp, "\n");

            //uint32_t vertex = 3;
            //while (vertex <= meshBlock1.size())
            //{
            //    fprintf(dmp, "f %u/%u %u/%u %u/%u\n", vertex-2, vertex - 2, vertex - 1, vertex - 1, vertex, vertex);
            //    vertex+=3;
            //}
            uint32_t n_tri = 0;
            
            std::vector<MeshTriangle>& tris = parsedTriangles.size() > 0 ? parsedTriangles : triangles;

            int count = 0;
            for (MeshTriangle& tri : tris)
            {
                fprintf(dmp, "#nib = 0x0%x tri#=%u\n", tri.nibble, n_tri++);
                fprintf(dmp, "f %s %s %s\n", tri.v1().c_str(), tri.v2().c_str(), tri.v3().c_str());
                count++;
                if (count % 2 == 0)
                {
                    fprintf(dmp, "\n");
                }
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
            meshHeaders[i].parseFloatBlock(data);
            meshHeaders[i].readTriangleDataFromIndexArray(m_filename, i);
            meshHeaders[i].readTriangleData(data);
            //mesh.parseBlock1(data);
            printf("");
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
#define MULTIPLE
    std::vector<const char*> files;
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Pen1/Pen1_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Mugcup/Mugcup_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/RES_MDL_S_STAGE/gomibako_gomibako_1.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Teapot/Teapot_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/BaboCoin/BaboCoin_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Mugcup/Mugcup_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/YUBIWA/YUBIWA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/KOUSUI_BOTTLE/KOUSUI_BOTTLE_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Pen1/Pen1_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Denkyu/denkyu_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Daruma/Daruma_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Danberu/Danberu_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Eraser/Eraser_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Hasami/hasami_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Hotchkiss/hotchkiss_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Keitaidenwa/KEITAIDENWA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Lighter/lighter_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Matoryoshika/MATORYOSHIKA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Pen2/Pen2_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Yunomi/Yunomi_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/KANDENCHI_MARU/KANDENCHI_MARU_MASTER.geom.edge");

    //files.push_back("D:/trash panic/Dumps/Stage6Dmp/Laserbeam/Laserbeam_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage6Dmp/MONOLITH_T/MONOLITH_T_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage6Dmp/Gomibako_gold/gomibako_gomibako_1.geom.edge");

    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Tabaco/TABACO_notdoll_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Bat/Bat_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Soccerball/Soccerball_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Speaker/Speaker_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/INUGOYA/INUGOYA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Oven/Oven_MASTER.geom.edge");

    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/AGuitar/AGuitar_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/BaboCoin/BaboCoin_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/cake/cake_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Deckbrash/Deckbrash_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/HANGER/HANGER_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Mokusei_ISU/MOKUSEI_ISU_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/OldPC/OldPC_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/UEKI_BACHI/UEKIBACHI_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Wok/Wok_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/Woodshelf/Woodshelf_MASTER.geom.edge");
    //

    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/TeraKane/TERA_KANE_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Fuhaidama/FUHAIDAMA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/WoodBox/WoodBox_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Post/Post_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Toilet/Toilet_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Tire/Tire_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Coffin/coffinA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Kaiga/KAIGA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Bike/Bike_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Chero/Chero_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/WoodenHorse/WoodenHorse_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Wadaiko/wadaiko_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Suika/SUIKA_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/Taru/Taru_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage3Dmp/OldTv/OLDTV_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/RES_MDL_S_UI/tmp_base_tmp_Default.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/RES_MDL_S_STAGE/gomibako_vs_gomibako_1.geom.edge");

    files.push_back("D:/trash panic/Dumps/Stage1Dmp/Humberger/HUMBURGER_break_Mesh8.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/Humberger/HUMBURGER_break_Mesh3.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage1Dmp/PotetoStick/potetostick_MASTER.geom.edge");
    //files.push_back("D:/trash panic/Dumps/Stage2Dmp/RES_MDL_S_STAGE/gomibako_gomibako_1.geom.edge");














#endif    

#ifdef MULTIPLE
    for (const char* file : files)
    {
#endif
        std::string path = file;
        if (path.find_last_of("/") != std::string::npos)
        {
            path = path.substr(0, path.find_last_of("/")) + "/";
        }

        int geomsize;
        uint8_t* data = readfile(file, geomsize);
        std::string material = std::regex_replace(file, std::regex("geom.edge"), "mat.edge");

        int matsize;
        uint8_t* matdata = readfile(material, matsize);

        GeomMaterial m;
        m.parse(matdata);
        m.dumpMaterials(path);
        geommaterial = m;

        Geom g(file, geomsize);
        g.parse(data);
        g.parseMeshHeaders(data);
        g.parseMesh(data);
        g.dump_meshes();

#ifdef MULTIPLE
    }
#endif
}

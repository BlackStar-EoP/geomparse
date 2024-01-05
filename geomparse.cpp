#include <iostream>
#include <vector>
#include <assert.h>
#include <sstream>
#include <regex>
#include "half.hpp"
#include <fstream>

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

inline int16_t Reversei16(int16_t value)
{
    return (((value & 0x00FF) << 8) |
        ((value & 0xFF00) >> 8));
}


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

int16_t parsei16(uint8_t* data, uint32_t& offset)
{
    int16_t* buf = (int16_t*)&data[offset];
    offset += sizeof(int16_t);
    return Reverse16(*buf);
}

float parsef8(uint8_t* data, uint32_t& offset)
{
    const float scale = 1.0f / 255.0f;

    float val = (uint32_t)data[offset] * scale;
    ++offset;
    return val;
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
    std::string filename;

    std::string name()
    {
        if (textures.size() > 0)
            return textures[0].name;

        return "NO_TEXTURE";
    }

    std::string getfilename()
    {
        size_t lastslash = filename.find_last_of("/");
        if (lastslash != std::string::npos)
        {
            filename = filename.substr(lastslash + 1, filename.length() - lastslash - 1);
        }

        std::string material = std::regex_replace(filename, std::regex(".mat.edge"), "");
        std::stringstream ss;
        assert(textures.size() > 0);
        ss << id << "_" << material << ".mtl";
        return ss.str();
    }

    void dumpMaterial(const std::string& path)
    {
        if (textures.size() > 0)
        {
            std::string filename = path + getfilename();
            FILE* fp = fopen(filename.c_str(), "w+");
            fprintf(fp, "newmtl %s\n", textures[0].name.c_str());
            fprintf(fp, "Ka 1.000000 1.000000 1.000000\n");
            fprintf(fp, "Kd 1.000000 1.000000 1.000000\n");
            fprintf(fp, "Ks 0.000000 0.000000 0.000000\n");
            fprintf(fp, "map_Kd %s\n", textures[0].texfile.c_str());
            if (textures.size() > 1)
            {
                fprintf(fp, "norm %s\n", textures[1].texfile.c_str());
            }

            if (textures.size() > 2)
            {
                printf("More than 2 textures, investigate!\n");
            }
            fclose(fp);
        }
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
    
    GeomMaterial(const std::string& filename)
    : m_filename(filename)
    {
    }


    void parse(uint8_t* data)
    {
        uint32_t offset = 0u;
        num_materials = parse32(data, offset);
        for (uint32_t m = 0; m < num_materials; ++m)
        {
            GeomMaterialEntry e;
            e.texcount = parse32(data, offset);
            e.id = m;
            e.filename = m_filename;
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
    std::string m_filename;
};

GeomMaterial geommaterial("");

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
    uint32_t t_, tt_, ttt_;
    MeshTriangle(uint32_t v1, uint32_t v2, uint32_t v3)
        : t_(v1), tt_(v2), ttt_(v3)
    {}

    std::string v1()
    {
        std::stringstream ss;
        ss << (t_ + 1) << "/" << (t_ + 1) << "/" << (t_ + 1);
        return ss.str();
    }
    std::string v2()
    {
        std::stringstream ss;
        ss << (tt_ + 1) << "/" << (tt_ + 1) << "/" << (tt_ + 1);
        return ss.str();
    }
    std::string v3()
    {
        std::stringstream ss;
        ss << (ttt_ + 1) << "/" << (ttt_ + 1) << "/" << (ttt_ + 1);
        return ss.str();
    }

    bool operator == (const MeshTriangle& t2) const
    {
        return (t_ == t2.t_) &&
               (tt_ == t2.tt_) &&
               (ttt_ == t2.ttt_);
    }
};

struct MeshVertex
{
    float vx, vy, vz;
    float tx, ty;
    float nx, ny, nz;
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

struct vec3
{
    float x_;
    float y_;
    float z_;

    vec3(float x, float y, float z)
    : x_(x), y_(y), z_(z)
    {
    }

    float magnitude()
    {
        return sqrt((x_ * x_) + (y_ * y_) + (z_ * z_));
    }

    void normalize()
    {
        float length = magnitude();
        if (length > 0)
        {
            x_ /= length;
            y_ /= length;
            z_ /= length;
        }
    }
};

struct GeomMeshHeader
{
    uint32_t signature;
    uint16_t unk1;
    uint8_t unk2;
    uint8_t materialId;
    uint16_t unk3;
    uint16_t numIndices;
    uint32_t allFF;

    uint32_t meshTrianglesAddress;
    uint16_t meshTrianglesSize;
    uint8_t* m_triangle_data;

    uint16_t padding1;

    uint32_t meshBlock1Address;
    uint32_t meshBlock1EndAddress;
    uint16_t meshBlock1Length;
    uint16_t padding2;

    uint32_t normalBlockLength;
    uint32_t unk32_2;

    uint32_t textureBlock1Address;
    uint32_t textureBlock1Length;

    static const uint32_t NUM_OFFSETS = 19;

    uint32_t offsets[NUM_OFFSETS];

    uint32_t num_vertices;
    uint32_t num_tex_coords;

    GeomAABB aabb_;

    std::vector<vec3> normals;

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
        numIndices = parse16(data, offset);
        allFF = parse32(data, offset);

        meshTrianglesAddress = parse32(data, offset);
        meshTrianglesSize = parse16(data, offset);
        padding1 = parse16(data, offset);

        meshBlock1Address = parse32(data, offset);
        meshBlock1EndAddress = parse32(data, offset);
        meshBlock1Length = parse16(data, offset);
        
        num_vertices = meshBlock1Length / 4 / 3;

        padding2 = parse16(data, offset);

        normalBlockLength = parse32(data, offset);
        unk32_2 = parse32(data, offset);

        textureBlock1Address = parse32(data, offset);
        textureBlock1Length = parse32(data, offset);
        num_tex_coords = textureBlock1Length / 2 / 2;

        for (uint32_t i = 0; i < NUM_OFFSETS; ++i)
        {
            offsets[i] = parse32(data, offset);
        }
    }

    std::vector<uint16_t> readVariableBitArray(uint8_t* variableBitIndices, uint32_t numBitsPerValue, uint32_t numVarBitIndices)
    {
        std::vector<uint16_t> parsedVariableBitIndices;
        uint32_t total = (numVarBitIndices + 0x1F) & 0xFFFFFFE0;
        uint32_t offset = (total - 1) * numBitsPerValue;
        for (uint32_t i = total; i > 0; --i)
        {
            uint8_t* start = variableBitIndices + (offset / 8);

            uint32_t first = start[0];
            uint32_t second = start[1];
            uint32_t third = start[2];

            uint32_t output = (first << 24) |
                (second << 16) |
                (third << 8);

            output <<= (offset & 0x07);

            parsedVariableBitIndices.insert(parsedVariableBitIndices.begin(), output >> (32 - numBitsPerValue));
            offset -= numBitsPerValue;
        }

        return parsedVariableBitIndices;
    }

    std::vector<uint16_t> read1bArray(const std::vector<uint16_t>& variableBitIndices, const uint8_t* prefaceData, uint32_t numIndices)
    {
        std::vector<uint16_t> decodedIndices;
        const uint8_t MASK_INITIAL = 0x80;

        uint16_t indexValue = 0;

        uint8_t mask = MASK_INITIAL;
        uint32_t index = 0;
        uint32_t prefaceDataIndex = 0;
        const uint32_t count = (numIndices + 0x0F) & 0xFFFFFFF0;

        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t currentPrefaceByte = prefaceData[prefaceDataIndex];

            if ((currentPrefaceByte & mask) == 0)
            {
                decodedIndices.push_back(indexValue++);
            }
            else
            {
                decodedIndices.push_back(variableBitIndices[index++]);
            }

            mask >>= 1;

            if (mask == 0)
            {
                mask = MASK_INITIAL;
                prefaceDataIndex++;
                if (prefaceDataIndex >= (numIndices / 8))
                {
                    break;
                }
            }
        }

        return decodedIndices;
    }

    void readBackRefIndices(std::vector<uint16_t>& indices, uint32_t numIndices, uint16_t backRefOffset)
    {
        const uint8_t NUM_BACKREFS = 8;

        uint16_t backRefs[NUM_BACKREFS];
        memset(backRefs, 0, sizeof(backRefs));

        uint32_t count = ((numIndices + 0x1F) & 0xFFFFFFE0) / 8;
        for (uint32_t i = 0; i < count; i++)
        {
            for (uint32_t backref = 0; backref < NUM_BACKREFS; backref++)
            {
                backRefs[backref] = indices[(i * NUM_BACKREFS) + backref] - backRefOffset + backRefs[backref];
                indices[(i * NUM_BACKREFS) + backref] = backRefs[backref];
            }
        }
    }

    std::vector<uint16_t> buildFaces(const std::vector<uint16_t>& indices, uint8_t* faceData, uint32_t numTris)
    {
        std::vector<uint16_t> indexArray;
        const uint8_t TRIS_PER_BYTE = 4;
        const uint8_t BITS_PER_TRIANGLE = 2;
        const uint32_t TOTALTRIS = (numTris + 7) & 0xFFFFFFF8;

        uint32_t index = 0;
        uint32_t faceIndex = 0;
        uint32_t faceDataSize = TOTALTRIS / TRIS_PER_BYTE;
        for (uint32_t face = 0; face < faceDataSize; face++)
        {
            uint8_t faceByte = faceData[faceIndex++];
            for (uint32_t tri = 0; tri < TRIS_PER_BYTE; ++tri)
            {
                uint8_t operation = faceByte & 0xC0;
                uint32_t last = indexArray.size();
                switch (operation)
                {
                case 0xC0: // new triangle
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    break;

                case 0x0: // backref -3 -1 0
                    indexArray.push_back(indexArray[last - 3]);
                    indexArray.push_back(indexArray[last - 1]);
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    break;

                case 0x40: // backref -1 -2 0
                    indexArray.push_back(indexArray[last - 1]);
                    indexArray.push_back(indexArray[last - 2]);
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    break;

                case 0x80: // backref -2 -3 0
                    indexArray.push_back(indexArray[last - 2]);
                    indexArray.push_back(indexArray[last - 3]);
                    indexArray.push_back(indices[index++]); if (index >= indices.size()) return indexArray;
                    break;

                }

                faceByte <<= BITS_PER_TRIANGLE;
            }
        }

        return indexArray;
    }

    void parseIndexArray(const uint8_t* data)
    {
        m_triangle_data = new uint8_t[meshTrianglesSize];
        memset(m_triangle_data, 0, meshTrianglesSize);
        memcpy(m_triangle_data, data + meshTrianglesAddress, meshTrianglesSize);

        uint32_t readOffset = 0;
        uint32_t numVarBitIndices = parse16(m_triangle_data, readOffset);
        uint16_t backRefOffset = parse16(m_triangle_data, readOffset);
        uint32_t num1BitIndices = parse16(m_triangle_data, readOffset) * 8;
        uint8_t variableIndexBitSize = parse8(m_triangle_data, readOffset);

        uint32_t numTriangles = numIndices / 3;
        uint32_t offsetFaceBytes = ((num1BitIndices + 7) / 8) + 8;
        uint32_t numFaceBytes = (((numTriangles + numTriangles) + 7) / 8);
        uint32_t offsetArrayVarBit = offsetFaceBytes + numFaceBytes;

        std::vector<uint16_t> variableBitIndices = readVariableBitArray(m_triangle_data + offsetArrayVarBit, variableIndexBitSize, numVarBitIndices);
        readBackRefIndices(variableBitIndices, numVarBitIndices, backRefOffset);
        std::vector<uint16_t> decodedIndices = read1bArray(variableBitIndices, m_triangle_data + 8, num1BitIndices);
        std::vector<uint16_t> indexArray = buildFaces(decodedIndices, m_triangle_data + offsetFaceBytes, numTriangles);

        for (uint32_t i = 0; i < numTriangles; ++i)
        {
            uint32_t i1 = i * 3;
            uint32_t i2 = (i * 3) + 1;
            uint32_t i3 = (i * 3) + 2;
            triangles.push_back(MeshTriangle(indexArray[i1], indexArray[i2], indexArray[i3]));
        }
        delete[] m_triangle_data;
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
                parsedTriangles.push_back(MeshTriangle(i1, i2, i3));
            }
        }

    }

    void parseFloatBlock(uint8_t* data)
    {
        uint32_t offset = meshBlock1EndAddress;

        for (uint32_t i = 0; i < num_vertices; ++i)
        {
            float nx = parsef8(data, offset) - 0.5f;
            float ny = parsef8(data, offset) - 0.5f;
            float nz = parsef8(data, offset) - 0.5f;

            float tx = parsef8(data, offset) - 0.5f;
            float ty = parsef8(data, offset) - 0.5f;
            float tz = parsef8(data, offset) - 0.5f;


            vec3 normal(nx, ny, nz);
            normal.normalize();
            normals.push_back(normal);
        }

        for (uint32_t i = 0; i < num_vertices; ++i)
        {
            meshBlock1[i].nx = normals[i].x_;
            meshBlock1[i].ny = normals[i].y_;
            meshBlock1[i].nz = normals[i].z_;
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
            if (materialId < geommaterial.materialEntries.size())
            {
                GeomMaterialEntry mat = geommaterial.materialEntries[materialId];

                fprintf(dmp, "mtllib %s\n", mat.getfilename().c_str());
                fprintf(dmp, "usemtl %s\n", mat.name().c_str());
            }
            uint32_t index = 1;
            for (uint32_t v = 0; v < num_vertices; ++v)
            {
                MeshVertex& vertex = meshBlock1[v];
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
                fprintf(dmp, "vt %f %f\n", vertex.tx, vertex.ty * -1);
                fprintf(dmp, "vn %f %f %f\n\n", vertex.nx, vertex.ny, vertex.nz);
                index++;
            }

            fprintf(dmp, "\n");

            uint32_t n_tri = 0;

            std::vector<MeshTriangle>& tris = parsedTriangles.size() > 0 ? parsedTriangles : triangles;

            int count = 0;
            for (MeshTriangle& tri : tris)
            {
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

    void parseMesh(uint8_t* data, bool readIdx)
    {
        for (int i = 0; i < meshHeaders.size(); ++i)
        {
            meshHeaders[i].parseBlock1(data);
            meshHeaders[i].parseFloatBlock(data);
            if (readIdx)
                meshHeaders[i].readTriangleDataFromIndexArray(m_filename, i);
            meshHeaders[i].parseIndexArray(data);
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
    {
        printf("Usage geomparse mesh\n");
        return -1;
    }
    std::string file = argv[1];
#else
    std::vector<std::string> files;
    //files.push_back("D:/trash panic/reveng/Stage2_Geom.dmp/Bluerayrecoder/Bluerayrecoder_damage_Mesh.geom.edge");
    //files.push_back("D:/trash panic/reveng/Stage2_Geom.dmp/Bluerayrecoder/Bluerayrecoder_break_Mesh5.geom.edge");
    //files.push_back("D:/trash panic/reveng/Stage5_Geom.dmp/Yuden/YUDEN_MASTER.geom.edge");
    //files.push_back("D:/trash panic/reveng/Stage4_Geom.dmp/RES_MDL_S_STAGE/gomibako_gomibako_1.geom.edge");
    //files.push_back("D:/trash panic/reveng/Stage4_Geom.dmp/RES_MDL_S_STAGE/huta_huta_3.geom.edge");
    //files.push_back("d:/trash panic/reveng/Stage5_Geom.dmp/RES_MDL_S_UI/tmp_tmp_Default.geom.edge");
    //files.push_back("d:/trash panic/reveng/Stage2_Geom.dmp/LCTV/LCTV_MASTER.geom.edge");
    //files.push_back("d:/trash panic/reveng/Stage1_Geom.dmp/BaboCoin/BaboCoin_MASTER.geom.edge");
    //files.push_back("D:/trash panic/reveng/2P_vs_Geom.dmp/SMM/SMM_SLIP_anim.geom.edge");
    
    //files.push_back("D:/trash panic/reveng/2P_vs_Geom.dmp/Post/Post_break_stone.geom.edge");
    //files.push_back("D:/trash panic/reveng/2P_vs_Geom.dmp/Post/Post_damage_damage.geom.edge");
    //files.push_back("D:/trash panic/reveng/Stage1_Geom.dmp/Humberger/HUMBURGER_break_Mesh1.geom.edge");
    //files.push_back("d:/trash panic/reveng/Stage1_Geom.dmp/Teapot/Teapot_MASTER.geom.edge");
    //files.push_back("D:/trash panic/reveng/Title_Geom.dmp/PressStart/PRESS_START_Default.geom.edge");
    
    files.push_back("D:/trash panic/reveng/Mission_1_Geom.dmp/Drumcan/Drumcan_damage_damage.geom.edge");

//#define DECODE_ONLY
#define MULTIPLE

#endif    


#ifdef MULTIPLE
    for (const std::string& file: files)
    {
#endif
        try
        {
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

            GeomMaterial m(material);
            m.parse(matdata);
            m.dumpMaterials(path);
            geommaterial = m;

            Geom g(file, geomsize);
            g.parse(data);
            g.parseMeshHeaders(data);
            bool readIdx = false;
            g.parseMesh(data, readIdx);

        
#ifndef DECODE_ONLY
            g.dump_meshes();
#endif
        }
        catch (...)
        {
            printf("Exception thrown when parsing %s\n", file.c_str());
        }

#ifdef MULTIPLE
        }
#endif
}

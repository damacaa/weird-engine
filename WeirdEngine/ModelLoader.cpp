#include "ModelLoader.h"

void ModelLoader::calcBoundaries(Vector3D v)
{
    this->maxX = fmax(this->maxX, v.X());
    this->maxY = fmax(this->maxY, v.Y());
    this->maxZ = fmax(this->maxZ, v.Z());
    this->minX = fmin(this->minX, v.X());
    this->minY = fmin(this->minY, v.Y());
    this->minZ = fmin(this->minZ, v.Z());
}

Triangle ModelLoader::center(Triangle t)
{
    Vector3D modelCenter(this->minX + this->getWidth() / 2.0,
        this->minY + this->getHeight() / 2.0,
        this->minZ + this->getLength() / 2);

    Triangle centeredTriangle(
        t.GetVertex1() - modelCenter,
        t.GetVertex2() - modelCenter,
        t.GetVertex3() - modelCenter,
        t.GetNormal1(),
        t.GetNormal2(),
        t.GetNormal3());
 
    return centeredTriangle;
}

Vector3D ModelLoader::parseObjLineToVector3D(const string& line)
{
    string typeOfPoint;
    float xCoordinate, yCoordinate, zCoordinate;
    istringstream stringStream(line);
    stringStream >> typeOfPoint >> xCoordinate >> yCoordinate >> zCoordinate;
    Vector3D vectorPoint(xCoordinate, yCoordinate, zCoordinate);
    return vectorPoint * this->getScale();
}

Triangle ModelLoader::parseObjTriangle(const string& line)
{
    char c;
    int idxVertex1, idxVertex2, idxVertex3;
    int idxNormal1, idxNormal2, idxNormal3;

    istringstream stringStream(line);
    stringStream >> c;
    stringStream >> idxVertex1 >> c >> c >> idxNormal1;
    stringStream >> idxVertex2 >> c >> c >> idxNormal2;
    stringStream >> idxVertex3 >> c >> c >> idxNormal3;

    Vector3D vertex1 = this->vertexes[idxVertex1 - 1];
    Vector3D vertex2 = this->vertexes[idxVertex2 - 1];
    Vector3D vertex3 = this->vertexes[idxVertex3 - 1];
    Vector3D normal = this->normals[idxNormal1 - 1];

    Triangle parsedTriangle(vertex1, vertex2, vertex3, normal, normal, normal);

    return parsedTriangle;
}

void ModelLoader::LoadModel(const string& ruta)
{
    try
    {
        ifstream objFile(ruta);
        if (objFile.is_open())
        {
            string line;
            int count = 0;
            while (getline(objFile, line))
            {
                if (line[0] == 'v' && line[1] == 'n')
                {
                    Vector3D normal = this->parseObjLineToVector3D(line);
                    this->normals.push_back(normal);
                }
                else if (line[0] == 'v')
                {
                    Vector3D vertex = this->parseObjLineToVector3D(line);
                    this->calcBoundaries(vertex);
                    this->vertexes.push_back(vertex);
                }
                else if (line[0] == 'f')
                {
                    Triangle triangle = this->parseObjTriangle(line);
                    this->model.AddTriangle(this->center(triangle));
                    //this->model.AddTriangle(triangle)
                }
            }
            objFile.close();
            cout << "Modelo cargado correctamente" << ruta << endl;
        }
        else
        {
            cout << "No se ha podido abrir el archivo" << ruta << endl;
        }

    }
    catch (exception& ex)
    {
        cout << "Excepción al procesar el archivo: " << ruta << endl;
        cout << ex.what() << endl;
    }
}

void ModelLoader::Clear()
{
    this->vertexes.clear();
    this->normals.clear();
    this->model.Clear();
}

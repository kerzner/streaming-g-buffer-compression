#ifndef CAMERAPATH_H
#define CAMERAPATH_H

#include "DXUT.h"
#include "App.h"
#include <vector>
using std::vector;

struct CameraParams
{
    D3DXVECTOR3 eye;
    D3DXVECTOR3 at;
};

class CameraPath
{
public:
    CameraPath();
    ~CameraPath();
    void AddFrame(D3DXVECTOR3& eye, D3DXVECTOR3& at);
    CameraParams GetFrame(unsigned frame);
    void Save(const char* filename);
    void Load(const char* filename);

    unsigned GetFrameCount() { return mCameraParams->size(); }
    void SetActiveLights(unsigned lights) { mActiveLights=lights; }
    unsigned GetActiveLights() { return mActiveLights; }

protected:
    vector<CameraParams>* mCameraParams;
    unsigned mActiveLights;
};

#endif // CAMERAPATH_H
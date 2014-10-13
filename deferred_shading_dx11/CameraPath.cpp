#include "CameraPath.h"

CameraPath::CameraPath()
{
    mCameraParams = new vector<CameraParams>();
    mActiveLights = 0;
}

CameraPath::~CameraPath()
{
    SAFE_DELETE(mCameraParams);
}

void CameraPath::AddFrame(D3DXVECTOR3& eye, D3DXVECTOR3& at)
{
    CameraParams params;
    params.eye = eye;
    params.at = at;
    mCameraParams->push_back(params);
}

CameraParams CameraPath::GetFrame(unsigned frame)
{
    return mCameraParams->at(frame);
}

void CameraPath::Save(const char* filename)
{
    FILE* file;
    fopen_s(&file, filename, "wb");

    unsigned frames = mCameraParams->size();
    fwrite(&frames, sizeof(frames), 1, file);
    fwrite(&mActiveLights, sizeof(mActiveLights), 1, file);

    for (unsigned i = 0; i < frames; i++) {
        CameraParams params = mCameraParams->at(i);
        fwrite(&params, sizeof(params), 1, file);
    }

    fclose(file);
}

void CameraPath::Load(const char* filename)
{
    if (mCameraParams) {
        SAFE_DELETE(mCameraParams);
        mCameraParams = new vector<CameraParams>();
    }

    FILE* file;
    fopen_s(&file, filename, "rb");

    unsigned frames;
    fread(&frames, sizeof(frames), 1, file);
    fread(&mActiveLights, sizeof(mActiveLights), 1, file);

    for (unsigned i = 0; i < frames; i++) {
        CameraParams params;
        fread(&params, sizeof(params), 1, file);
        mCameraParams->push_back(params);
    }

    fclose(file);
}
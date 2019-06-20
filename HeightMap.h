#pragma once

struct HeightField
{
	HeightField(int nDimension) :
		m_nDimension(nDimension)
	{
		m_pHeightField = new float[nDimension*nDimension];
	}

	~HeightField()
	{
		delete[] m_pHeightField;
	}

	float GetValue(int x, int y);
	void SetValue(int x, int y, float val);

	void FillRandom(float waterplane);
	void FillAverage(float value);
	void Erode();	

	float* m_pHeightField;
	int m_nDimension;
};

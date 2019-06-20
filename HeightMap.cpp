#include "HeightMap.h"

#include "../JuceLibraryCode/JuceHeader.h"
#include <algorithm>

float HeightField::GetValue(int x, int y)
{
	int _x = jlimit(0, m_nDimension, x);
	int _y = jlimit(0, m_nDimension, y);

	return m_pHeightField[_y * m_nDimension + _x];
}

void HeightField::SetValue(int x, int y, float val)
{
	m_pHeightField[y * m_nDimension + x] = val;
}

void HeightField::Erode()
{
	float* pMinus1 = m_pHeightField;
	float* pCurrentVoxel = m_pHeightField;
	float* pPlus1 = m_pHeightField + m_nDimension;

	float* pTempHeightField = new float[m_nDimension*m_nDimension];

	float* pFilteredVoxel = pTempHeightField;
	for (int y = 0; y < m_nDimension; ++y)
	{
		for (int x = 0; x < m_nDimension; ++x, ++pCurrentVoxel, ++pFilteredVoxel)
		{
			float flMinVoxelValue = *pCurrentVoxel;

			if (x < m_nDimension - 1)
			{
				flMinVoxelValue = std::min(flMinVoxelValue, *(pCurrentVoxel + 1));
			}
			else
			{
				flMinVoxelValue = 0.f;
			}
			if (x > 0)
			{
				flMinVoxelValue = std::min(flMinVoxelValue, *(pCurrentVoxel - 1));
			}
			else
			{
				flMinVoxelValue = 0.f;
			}

			if (y > 0)
			{
				flMinVoxelValue = std::min(flMinVoxelValue, *pMinus1);
				if (x < m_nDimension - 1)
				{
					flMinVoxelValue = std::min(flMinVoxelValue, *(pMinus1 + 1));
				}
				if (x > 0)
				{
					flMinVoxelValue = std::min(flMinVoxelValue, *(pMinus1 - 1));
				}
				++pMinus1;
			}
			else
			{
				flMinVoxelValue = 0.f;
			}

			if (y < m_nDimension - 1)
			{
				flMinVoxelValue = std::min(flMinVoxelValue, *pPlus1);
				if (x < m_nDimension - 1)
				{
					flMinVoxelValue = std::min(flMinVoxelValue, *(pPlus1 - 1));
				}
				if (x > 0)
				{
					flMinVoxelValue = std::min(flMinVoxelValue, *(pPlus1 - 1));
				}
				++pPlus1;
			}
			else
			{
				flMinVoxelValue = 0.f;
			}

			if (flMinVoxelValue < 0.333f)
			{
				*pFilteredVoxel = (*pCurrentVoxel + flMinVoxelValue) / 2.f;
			}
			else
			{
				*pFilteredVoxel = *pCurrentVoxel;
			}
		}
	}

	delete[] m_pHeightField;
	m_pHeightField = pTempHeightField;
}

void HeightField::FillAverage(float value)
{
	std::fill_n(m_pHeightField, m_nDimension * m_nDimension, value);
}

void HeightField::FillRandom(float waterplane)
{
	Random rand(12345 * 13);

	const int nDimension = m_nDimension;

	float* pHeightField = new float[nDimension*nDimension];

	int lakeX = rand.nextInt(nDimension - 10);
	int lakeY = rand.nextInt(nDimension - 10);
	int r_sq = 100;

	float height = 0.f;
	float* pCurrentVoxel = pHeightField;
	for (int h = 0; h < nDimension; ++h)
	{
		height = 0.f;
		for (int w = 0; w < nDimension; ++w, ++pCurrentVoxel)
		{
			int d1 = w - lakeX;
			int d2 = h - lakeY;
			if (d1*d1 + d2*d2 < r_sq)
			{
				height = 0.f;
			}
			else
			{
				height = (rand.nextFloat() + height) * 0.5f;
			}

			*pCurrentVoxel = height;
		}
	}

	int riverPos = rand.nextInt(nDimension);
	pCurrentVoxel = pHeightField + riverPos;
	for (int h = 0; h < nDimension; ++h, pCurrentVoxel += nDimension)
	{
		*pCurrentVoxel = -0.3333f;
	}

	float* pMinus1 = pHeightField;
	pCurrentVoxel = pHeightField;
	float* pPlus1 = pHeightField + nDimension;

	float* pFilteredVoxel = m_pHeightField;
	for (int y = 0; y < nDimension; ++y)
	{
		for (int x = 0; x < nDimension; ++x, ++pCurrentVoxel, ++pFilteredVoxel)
		{
			*pFilteredVoxel = *pCurrentVoxel;

			if (x < nDimension - 1)
			{
				*pFilteredVoxel += *(pCurrentVoxel + 1);
			}
			if (x > 0)
			{
				*pFilteredVoxel += *(pCurrentVoxel - 1);
			}

			if (y > 0)
			{
				*pFilteredVoxel += *pMinus1;
				if (x < nDimension - 1)
				{
					*pFilteredVoxel += *(pMinus1 + 1);
				}
				if (x > 0)
				{
					*pFilteredVoxel += *(pMinus1 - 1);
				}
				++pMinus1;
			}
			if (y < nDimension - 1)
			{
				*pFilteredVoxel += *pPlus1;
				if (x < nDimension - 1)
				{
					*pFilteredVoxel += *(pPlus1 + 1);
				}
				if (x > 0)
				{
					*pFilteredVoxel += *(pPlus1 - 1);
				}
				++pPlus1;
			}

			*pFilteredVoxel /= 9;
		}
	}
	
	delete[] pHeightField;
}
/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include <algorithm>
#include <array>
#define _USE_MATH_DEFINES
#include <math.h>
#include <windows.h>

#include "HeightMap.h"
#include <xmmintrin.h>
#include <smmintrin.h>

#include "CubicSpline.h"

//#define PROFILE_ENABLE

#ifdef PROFILE_ENABLE
#include "Brofiler.h"
#endif // PROFILE_ENABLE

#include "VolumeNormalization.inl"

static bool bUseSIMD = true;
static bool bUseCubicSpline = true;

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainContentComponent   : public AudioAppComponent,
							   public ChangeListener,
							   public Button::Listener,
                               public Slider::Listener,
						       public KeyListener
							   //, public MouseListener
{
public:
    //==============================================================================
    MainContentComponent():
		m_ListenerPosX(0),
		m_ListenerPosY(0)
    {
        std::vector<CubicSplineTest::WorldSpace>control_points;

        control_points.push_back(CubicSplineTest::WorldSpace{ 0.f, 0.f, 0.f });
        control_points.push_back(CubicSplineTest::WorldSpace{ 10.f, 0.f, 0.f });
        control_points.push_back(CubicSplineTest::WorldSpace{ 10.f, 10.f, 0.f });
        control_points.push_back(CubicSplineTest::WorldSpace{ 20.f, 10.f, 0.f });

        CubicSplineTest::WorldSpace test_position{ 5.795f, 0.297f, 0.f };

        std::unique_ptr<CubicSplineTest::CubicBezierPath> bezier_path = 
            std::unique_ptr<CubicSplineTest::CubicBezierPath>(
                new CubicSplineTest::CubicBezierPath(&control_points[0], (int)control_points.size()));

        std::unique_ptr<CubicSplineTest::ClosestPointSolver> solver =
            std::unique_ptr<CubicSplineTest::ClosestPointSolver>(new CubicSplineTest::ClosestPointSolver());

        CubicSplineTest::WorldSpace solution = bezier_path->ClosestPointToPath(test_position, solver.get());

		addAndMakeVisible(&guiPlayButton);
		guiPlayButton.setButtonText("Load");
		guiPlayButton.addListener(this);
		guiPlayButton.setColour(TextButton::buttonColourId, Colours::green);

		addAndMakeVisible(&guiGenHeightMap);
		guiGenHeightMap.setButtonText("Gen Height Map");
		guiGenHeightMap.addListener(this);
			
		addAndMakeVisible(&guiLeftSP);
		guiLeftSP.setText("Left:  ", NotificationType::dontSendNotification);
		addAndMakeVisible(&guiRightSP);
		guiRightSP.setText("Right: ", NotificationType::dontSendNotification);
		addAndMakeVisible(&guiPower);
		guiPower.setText("Power: ", NotificationType::dontSendNotification);
		addAndMakeVisible(&guiFilterCutoff);
		guiFilterCutoff.setText("Filter Hz: ", NotificationType::dontSendNotification);

		addAndMakeVisible(&guiHeightFieldComponent);
		guiHeightFieldComponent.setBounds(0, 0, 400, 400);
		guiHeightFieldComponent.setWantsKeyboardFocus(true);
		guiHeightFieldComponent.setMouseClickGrabsKeyboardFocus(true);
		guiHeightFieldComponent.addKeyListener(this);		
		guiHeightFieldComponent.addMouseListener(this, true);

        addAndMakeVisible(&guiHumiditySlider);
        guiHumiditySlider.setRange(1.0, 100.0, 0.25);
        guiHumiditySlider.setTextValueSuffix(" %");
        guiHumiditySlider.setValue(60.0);
        guiHumiditySlider.addListener(this);

        addAndMakeVisible(guiHumidityLabel);
        guiHumidityLabel.setText("Humidity", dontSendNotification);
        guiHumidityLabel.attachToComponent(&guiHumiditySlider, true);

		setSize (800, 600);

		formatManager.registerBasicFormats();
		//transportSource.addChangeListener(this);   // [2]


        // specify the number of input and output channels that we want to open
        setAudioChannels (0, 2);
		setWantsKeyboardFocus(true);

		AddAudioSource(SBT_Pond);
		AddAudioSource(SBT_Ocean);
		AddAudioSource(SBT_River);		
    }

    double CalcAbsorptionCoefficient(double hz, double humidityPercent, double tempatureF, double pressurePa = 101325.0)
    {
        double flCutoffFrequency = hz;
        
        //double flTempatureK = 283.15; // (50 F)
        double flTempatureK = (tempatureF + 459.60) * 5 / 9.0;
        double fl20K = 293.15;
        double flTempNormalized = flTempatureK / fl20K;
        
        double flPressureSeaLevelPa = 101325.0;
        double flPressureNorm = pressurePa / flPressureSeaLevelPa;

        double flPressureCoefficient = (double)1.84e-11;

        double flTempNormInvCube = 1.0 / (flTempNormalized * flTempNormalized * flTempNormalized);

        double flApproxNitrogenFreq = 200.0;
        double flApproxOxygenFreq = 25000.0;
        
        double flCsat = -6.8346 * pow(273.16 / flTempatureK, 1.261) + 4.6151;
        double flPsat = pow(10, flCsat);
        double flHumidity = humidityPercent * flPsat / flPressureNorm;

        double flNitrogenHumidityFactor = 9 + 280 * flHumidity * exp(-4.170 * (pow(flTempNormalized, -1.0 / 3) - 1.0));
        double flNitrogenFreq = flPressureNorm * 1.0 / sqrt(flTempNormalized) * flNitrogenHumidityFactor;

        double flOxygenHumidyFactor = 24 + 40400 * flHumidity * (0.02 + flHumidity) / (0.391 + flHumidity);
        double flOxygenFreq = flPressureNorm * flOxygenHumidyFactor;

        double flNitrogenRelaxtion = 0.1068 * exp(-3352.0 / flTempatureK);
        flNitrogenRelaxtion /= (flNitrogenFreq + flCutoffFrequency * (flCutoffFrequency / flNitrogenFreq));

        double flOxygenRelaxtion = 0.01275 * exp(-2239.10 / flTempatureK);
        flOxygenRelaxtion /= (flOxygenFreq + (flCutoffFrequency * flCutoffFrequency) / flOxygenFreq);

        double flPressureFactor = flPressureCoefficient / flPressureNorm;
        double flRelaxationFactor = (flNitrogenRelaxtion + flOxygenRelaxtion) * flTempNormInvCube;

        double flAbsorption = 8.686 * sqrt(flTempNormalized) * (flPressureFactor + flRelaxationFactor);
        flAbsorption *= flCutoffFrequency;
        flAbsorption *= flCutoffFrequency;

        return flAbsorption;
    }

	double DistanceToFilterCutoff(double distance, double relativeCoefficient, double humidityPercent, double tempatureF)
	{
		double absorptionCoefficient = 3.0 / distance;
        
        double tempatureK = (tempatureF + 459.60) * 5 / 9.0;
		double cutoffFrequency = AbsorptionToFrequency(absorptionCoefficient, humidityPercent, tempatureK);
        double cutoffFrequency2 = AbsorptionToFrequency(absorptionCoefficient, humidityPercent - 20.0, tempatureK); // lower
        double cutoffFrequency3 = AbsorptionToFrequency(absorptionCoefficient, humidityPercent + 10.0, tempatureK); // higher cutoff
        double cutoffFrequency4 = AbsorptionToFrequency(absorptionCoefficient, humidityPercent + 25.0, tempatureK); // higher cutoff

        double verify = CalcAbsorptionCoefficient(cutoffFrequency, humidityPercent, tempatureF);
		return cutoffFrequency;
	}

	double AbsorptionToFrequency(double absorptionCoefficient, double humidityPercent, double tempatureK, double pressurePa = 101325.0)
	{
		double pressureSeaLevelPa = 101325.0;
		double pressureNorm = pressurePa / pressureSeaLevelPa;

		double tempNormalized = tempatureK / 293.15;

		double csat = -6.8346 * pow(273.16 / tempatureK, 1.261) + 4.6151;
		double psat = pow(10, csat);
		double humidity = humidityPercent * psat / pressureNorm;
		
		double nitrogenHumidityFactor = 9 + 280 * humidity * exp(-4.170 * (pow(tempNormalized, -1.0 / 3) - 1.0));
		double nitrogenHz = pressureNorm * 1.0 / sqrt(tempNormalized) * nitrogenHumidityFactor;

		double oxygenHumidyFactor = 24 + 40400 * humidity * (0.02 + humidity) / (0.391 + humidity);
		double oxygenHz = pressureNorm * oxygenHumidyFactor;

		double tempNormInvCube = 1.0 / (tempNormalized * tempNormalized * tempNormalized);
		double nitrogenRelaxtionCoefficient = tempNormInvCube * 0.1068 * exp(-3352.0 / tempatureK);
		double oxygenRelaxtionCoefficient = tempNormInvCube * 0.01275 * exp(-2239.10 / tempatureK);

		double pressureCoefficient = (double)1.84e-11 / pressureNorm;
		double outerCoefficient = 8.686 * sqrt(tempNormalized);

		double a1 = outerCoefficient * pressureCoefficient;
		double a2 = outerCoefficient * nitrogenRelaxtionCoefficient;
		double a3 = outerCoefficient * oxygenRelaxtionCoefficient;
		double a4 = absorptionCoefficient;

		double nitrogenHzSq = nitrogenHz*nitrogenHz;
		double oxygenHzSq = oxygenHz*oxygenHz;

		double a = a1;
		double b = a1 * nitrogenHzSq + a1 * oxygenHzSq + a2 * nitrogenHz + a3 * oxygenHz - a4;
		double c = a1 * nitrogenHzSq * oxygenHzSq + a2 * oxygenHzSq * nitrogenHz + a3 * nitrogenHzSq * oxygenHz - a4 * nitrogenHzSq - a4 * oxygenHzSq;
		double d = -a4 * oxygenHzSq * nitrogenHzSq;


		// Cubic Solver

		double p = (3 * a*c - b*b) / (3 * a*a);
		double q = ((2.0 * b*b*b) - (9 * b*a*c) + (27 * a*d*a)) / (27 * a*a*a);

		double theta = (3 * q * sqrt(-3 / p)) / (2 * p);
		double t0 = 2 * sqrt(-p / 3) * cos(acos(theta) / 3);
		// Debugging		
		double t2 = -2 * sqrt(-p / 3) * cos(acos(-theta) / 3);
		double t1 = -t0 - t2;		

		t0 -= b / (3 * a);

		double frequencyHz = sqrt(t0);

		return frequencyHz;
	}

struct Butterworth1Pole
{        
    float a1;
    float b0;
    float b1;

    float x1;
    float y1;

    bool bypass;

    Butterworth1Pole() :
        x1(0.0),
        y1(0.0),
        bypass(false) {}

    void Initialize(double cutoff_frequency, double sample_rate) 
    {
        const float blt_freq_warping = 1 / tan(M_PI * cutoff_frequency / sample_rate);

        const float a0 = 1 + blt_freq_warping;
        a1 = (1 - blt_freq_warping) / a0;
        b0 = 1 / a0;
        b1 = b0;
    }

    float process(float x)
    {
        if (!bypass)
        {
            y1 = b0*x + b1*x1 - a1*y1;
            x1 = x;

            return y1;
        }
        return x;
    }
};

	void CalcButterworth1Pole(double cutoffFrequency, double sampleRate, Butterworth1Pole* bw = nullptr)
	{
        bw->Initialize((float)cutoffFrequency, (float)sampleRate);

//		double normalizedFrequency = cutoffFrequency / sampleRate;
//		double bltGain = 1 / tan(double_Pi * normalizedFrequency); // 2 * pi * cutoff / (2 * sr)
//
//		double a0 = 1 + bltGain;
//		double a1 = (1 - bltGain) / a0;
//		double b0 = 1 / a0;
//		double b1 = b0;
//
//        if (bw)
//        {
//            bw->bypass = cutoffFrequency >= sampleRate / 2;
//            bw->a1 = (float)a1;
//            bw->b0 = (float)b0;
//            bw->b1 = (float)b1;            
//        }
//
//double blt_constant = 1 / tan(M_PI * cutoff_frequency / sample_rate);
//
//double a0 = 1 + bltGain;
//double a1 = (1 - bltGain) / a0;
//double b0 = 1 / a0;
//double b1 = b0;

		return;
	}
	
	double m_flCutoffAbsorptionAt5000HzDb = 0.f;	

    ~MainContentComponent()
    {
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        // This function will be called when the audio device is started, or when
        // its settings (i.e. sample rate, block size, etc) are changed.

        // You can use this function to initialise any resources you might need,
        // but be careful - it will be called on the audio thread, not the GUI thread.

        // For more details, see the help for AudioProcessor::prepareToPlay()
		//transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

        m_sampleRate = sampleRate;
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
		bufferToFill.clearActiveBufferRegion();
		const int channels = bufferToFill.buffer->getNumChannels();

		for (int i = 0; i < nNumSources; ++i)
		{
			if (m_pSources[i].m_bPlaying.get() == 0)
			{
				continue;
			}

			AudioSampleBuffer* pBuffer = m_pSources[i].m_eBufferType == SBT_Pond ? m_pPondBuffer.get() : 
				(m_pSources[i].m_eBufferType == SBT_Ocean ? m_pOceanBuffer.get() : m_pRiverBuffer.get());
			if (pBuffer == nullptr)
			{
				continue;
			}

			int nSamplesRemaining = bufferToFill.numSamples;
			int nSampleOffset = bufferToFill.startSample;
			int& nBufferIndex = m_pSources[i].m_nBufferIndex;
			float flGainL = m_pSources[i].m_flGainL;
			float flGainR = m_pSources[i].m_flGainR;

			while (nSamplesRemaining)
			{				
				int nBufferSamplesRemaining = pBuffer->getNumSamples() - nBufferIndex;
				int nSamplesToWrite = jmin(nSamplesRemaining, nBufferSamplesRemaining);

				for (int channel = 0; channel < channels; ++channel)
				{
                    const float * pReadBuffer = pBuffer->getReadPointer(channel, nBufferIndex);
                    float * pWriteBuffer = pBuffer->getWritePointer(channel, nBufferIndex);

                    for (int s = 0; s < nSamplesToWrite; ++s, ++pReadBuffer, ++pWriteBuffer)
                    {
                        *pWriteBuffer = m_bwFilter.process(*pReadBuffer);
                    }

					bufferToFill.buffer->addFrom(channel,
						nSampleOffset, 
						*pBuffer, 
						channel, 
						nBufferIndex, 
						nSamplesToWrite, 
						(channel == 0) ? flGainL : flGainR);
				}

				nSamplesRemaining -= nSamplesToWrite;
				nSampleOffset += nSamplesToWrite;
				nBufferIndex += nSamplesToWrite;

				if (nBufferIndex == pBuffer->getNumSamples())
				{
					nBufferIndex = 0;
				}
			}
		}

		//for (int i = 0; i < channels; ++i)
		//{
		//	const float* inBuffer = bufferToFill.buffer->getReadPointer(i, bufferToFill.startSample);
		//	float* outBuffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);

		//	for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
		//	{
		//		outBuffer[sample] = inBuffer[sample] * (i > 0 ? 0.25f : 1.f);
		//	}
		//}
    }

    void releaseResources() override
    {
        // This will be called when the audio device stops, or when it is being
        // restarted due to a setting change.

        // For more details, see the help for AudioProcessor::releaseResources()
		m_pPondBuffer.release();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
#ifdef PROFILE_ENABLE
		BROFILER_FRAME("PaintThread")
#endif

        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll (Colours::grey);		

        // You can add your drawing code here!
		//if (m_tHeightMap != nullptr)
		//{
		//	m_tHeightMap->setPixelAt(m_ListenerPosX.get(), m_ListenerPosY.get(), Colour(255,96,96));

		//	g.setImageResamplingQuality(Graphics::ResamplingQuality::highResamplingQuality);
		//	AffineTransform t;
		//	t = t.scaled(2.f, 2.f).translated(getWidth() / 2 - 200, 100);
		//	g.drawImageTransformed(*m_tHeightMap, t);
		//}
		if (m_tHeightMap != nullptr)
		{
			//m_tHeightMap->setPixelAt(m_ListenerPosX.get(), m_ListenerPosY.get(), Colour(255, 96, 96));
			const int x = m_ListenerPosX.get();
			const int y = m_ListenerPosY.get();
			bool positionUpdated = guiHeightFieldComponent.SetPosition(x, y);
			guiHeightFieldComponent.repaint();

            if (positionUpdated || m_bUpdateAbsorption)
			{                
                if (m_bUpdateAbsorption)
                {
                    m_flCutoffAbsorptionAt5000HzDb = CalcAbsorptionCoefficient(5000, m_humidity, 60);
                    m_bUpdateAbsorption = false;
                }
				guiPower.setText("Power: " + juce::String(guiHeightFieldComponent.m_power, 4),
					NotificationType::dontSendNotification);

                const double nearField = 1.0;
                int extent = m_tHeights->m_nDimension / 2;
                int dx = extent - x / 2;
                int dy = extent - y / 2;
                double distance = sqrt((double)dx*dx + (double)dy*dy);
                if (distance > nearField)
                {
                    m_filterCutoff = DistanceToFilterCutoff(distance, m_flCutoffAbsorptionAt5000HzDb, m_humidity, 60);
                    CalcButterworth1Pole(m_filterCutoff, m_sampleRate, &m_bwFilter);
                }
				guiFilterCutoff.setText("Filter Hz: " + juce::String(m_filterCutoff, 1),
					NotificationType::dontSendNotification);
			}
		}

		//juce::String leftSP =  "Left:  " + juce::String(m_pSources[0].m_flGainL, 4);
		//juce::String rightSP = "Right: " + juce::String(m_pSources[0].m_flGainR, 4);
		//g.drawText(leftSP, 430, 220, 80, 20, juce::Justification::centredLeft);
		//g.drawText(rightSP, 430, 242, 80, 20, juce::Justification::centredLeft);
    }

    void resized() override
    {
        // This is called when the MainContentComponent is resized.
        // If you add any child components, this is where you should
        // update their positions.
		guiPlayButton.setBounds(10, 40, getWidth() - 20, 20);
		guiGenHeightMap.setBounds(10, 70, getWidth() - 20, 20);

		guiLeftSP.setBounds(420, 220, 80, 12);
		guiRightSP.setBounds(420, 240, 80, 12);
		guiPower.setBounds(420, 260, 80, 12);
		guiFilterCutoff.setBounds(420, 280, 100, 12);

        guiHumiditySlider.setBounds(420, 310, 200, 20);
    }

	void mouseDown(const MouseEvent &event)
	{
		if (event.eventComponent == &guiHeightFieldComponent)
		{
			m_ListenerPosX = event.getMouseDownX();
			m_ListenerPosY = event.getMouseDownY();
			RefreshPosition();
		}
	}

    void sliderValueChanged(Slider* slider)
    {
        if (slider == &guiHumiditySlider)
        {
            m_humidity = (float)guiHumiditySlider.getValue();
            m_bUpdateAbsorption = true;
        }
    }

	#define KC_KEY_UP 65574
	#define KC_KEY_DOWN 65576
	#define KC_KEY_RIGHT 65575
	#define KC_KEY_LEFT	65573
	#define KC_SPACE 32

	bool keyPressed(const KeyPress &key, Component *originatingComponent)
	{
		int keyCode = key.getKeyCode();
		switch (keyCode)
		{
		case KC_SPACE:
			for (int i = 0; i < nNumSources; ++i)
			{
				m_pSources[i].m_bPlaying = 0;
			}			
			return true;
		case KC_KEY_UP:
		case (int)'W':
			--m_ListenerPosY;
			break;
		case KC_KEY_DOWN:
		case (int)'S':
			++m_ListenerPosY;
			break;
		case KC_KEY_LEFT:
		case (int)'A':
			--m_ListenerPosX;
			break;
		case KC_KEY_RIGHT:
		case (int)'D':
			++m_ListenerPosX;
			break;
		default:
			return false;
		}
		RefreshPosition();
		return true;
	}

	bool isWater(float value) { return value < 0.333f; }
	
	bool isOcean(int x, int y, int max, int attenuation)
	{
		return false;
		//return x <= attenuation || y <= attenuation
		//	|| max - attenuation <= x || max - attenuation <= y;
	}

	void RefreshPosition()
	{
		int listenerX = m_ListenerPosX.get()/2;
		int listenerY = m_ListenerPosY.get()/2;

		int attenDistance = 64;		

		double pondL = 0.0;
		double pondR = 0.0;
		double pondHiL = 0.0;
		double pondHiR = 0.0;
		double oceanL = 0.0;
		double oceanR = 0.0;

		int tiles = 0;

		// float energyPerMeter = 3 / (float_Pi * attenDistance * attenDistance); // approximate volume of a cone		
		double pondAttenuation = 100;		

		for (int y = listenerY - attenDistance; y < listenerY + attenDistance; ++y)
		{
			for (int x = listenerX - attenDistance; x < listenerX + attenDistance; ++x)
			{
				if (isWater(m_tHeights->GetValue(x, y)))
				{
					int dx = listenerX - x;
					int dy = listenerY - y;
					int dySq = dy * dy;
					int dSq = (dx*dx + dySq);
					double d = sqrt(dSq);
					double dAverage = d + 0.5f;

					++tiles;

					double nearField = 1.0;
					//double attenFactor = (1.0 - pondAttenuation * 0.00063) / (1.0 - pondAttenuation);
					double attenFactor = 0.0;
					double geometricAtten = (1.0 - attenFactor)/(dAverage > nearField ? dAverage : nearField) + attenFactor;

					if (geometricAtten < 0.0)
					{
						continue;
					}
					
					//double energy = 1.f - energyDecayPerMeter * dAverage;
                    double sphericalSpreadingFactor = 0.2821; // surface area of sphere log10
					//double energy = geometricAtten * sphericalSpreadingFactor; 
                    double energy = geometricAtten;
					double engAbsorbed = energy * pow(10.0, -(m_flCutoffAbsorptionAt5000HzDb * dAverage) / 20);
					double dotHalf = dySq != 0 ? 0.5 * abs((double)dy) / d : 0.0; // + 0 * dx

					if (dy == 0 && dx == 0)
					{
						dotHalf = 0.5;
					}

					if (x > listenerX) // Right Channel
					{
						if (isOcean(x, y, m_tHeights->m_nDimension, attenDistance))
						{
							oceanR += (1.0 - dotHalf)*energy;
							oceanL += dotHalf * energy;
						}
						else
						{
							pondR += (1.0 - dotHalf)*energy;
							pondL += dotHalf * energy;

							pondHiR += (1.0 - dotHalf)*engAbsorbed;
							pondHiL += dotHalf * engAbsorbed;
						}
					}
					else // Left Channel
					{
						if (isOcean(x, y, m_tHeights->m_nDimension, attenDistance))
						{
							oceanL += (1.0 - dotHalf)*energy;
							oceanR += dotHalf * energy;
						}
						else
						{
							pondL += (1.0 - dotHalf)*energy;
							pondR += dotHalf * energy;

							pondHiL += (1.0 - dotHalf)*engAbsorbed;
							pondHiR += dotHalf * engAbsorbed;
						}
					}
				}
			}
		}

		if (pondL > 0.0 || pondR > 0.0)
		{			
			//double pondRelativeVolume = pow(10, -10.0 / 20.0); // - 6 db headroom. mixed with -24 rms speech.
            double pondRelativeVolume = 1.0f;

			pondL = jmin(1.f, (float)(pondL * pondRelativeVolume));
			pondR = jmin(1.f, (float)(pondR * pondRelativeVolume));
			pondHiL = jmin(1.f, (float)(pondHiL * pondRelativeVolume));
			pondHiR = jmin(1.f, (float)(pondHiR * pondRelativeVolume));
			
				m_pSources[0].m_bPlaying = 1;
                m_pSources[0].m_flGainL = pondL;
				m_pSources[0].m_flGainR = pondR;

				guiLeftSP.setText("Left:  " + juce::String(m_pSources[0].m_flGainL, 4), 
					NotificationType::dontSendNotification);
				guiRightSP.setText("Right: " + juce::String(m_pSources[0].m_flGainR, 4), 
					NotificationType::dontSendNotification);

				double delta = pondL - pondHiL;
				double dampened = jmax( 1.0 - delta / pondL, 0.707);
				double dryPercent = (dampened - 0.707) / (1 - 0.707);
				float dryR = pondHiR / tiles;
		}
		else
		{
			m_pSources[0].m_bPlaying = 0;
		}
		if (oceanL > 0.0 || oceanR > 0.0)
		{
			m_pSources[1].m_bPlaying = 1;
			m_pSources[1].m_flGainL = (float)oceanL;
			m_pSources[1].m_flGainR = (float)oceanR;
		}
		else
		{
			m_pSources[1].m_bPlaying = 0;
		}
	}

	void changeListenerCallback(ChangeBroadcaster* source) override
	{
		//if (source == &transportSource)
		//{
		//}
	}

	void buttonClicked(Button* button) override
	{
		if (button == &guiPlayButton)  PlayButtonClicked();
		else if (button == &guiGenHeightMap)
		{
            //for (int i = 0; i < 4; ++i)
            //{
            //    RunLoudnessProfile();
            //}
			GenHeightMapClicked();
			guiHeightFieldComponent.grabKeyboardFocus();			
			guiHeightFieldComponent.setImage(*m_tHeightMap);
		}
	}
private:
    
	void PlayButtonClicked()
	{
		shutdownAudio();

		// Lake
		{
			//File file = File::getCurrentWorkingDirectory().getChildFile("lake_loop.wav");
			File file = File::getCurrentWorkingDirectory().getChildFile("river_loop.wav");
			ScopedPointer<AudioFormatReader> reader(formatManager.createReaderFor(file));

			if (reader != nullptr)
			{
				m_pPondBuffer = new AudioSampleBuffer();
				m_pPondBuffer->setSize(reader->numChannels, reader->lengthInSamples);
				reader->read(m_pPondBuffer.get(),
					0,
					reader->lengthInSamples,
					0,
					true,
					true);
			}
		}

		// Ocean
		{
			File file = File::getCurrentWorkingDirectory().getChildFile("ocean_loop.wav");
			ScopedPointer<AudioFormatReader> reader(formatManager.createReaderFor(file));

			if (reader != nullptr)
			{
				m_pOceanBuffer = new AudioSampleBuffer();
				m_pOceanBuffer->setSize(reader->numChannels, reader->lengthInSamples);
				reader->read(m_pOceanBuffer.get(),
					0,
					reader->lengthInSamples,
					0,
					true,
					true);
			}
		}

		// River
		{
			File file = File::getCurrentWorkingDirectory().getChildFile("river_loop.wav");
			ScopedPointer<AudioFormatReader> reader(formatManager.createReaderFor(file));

			if (reader != nullptr)
			{
				m_pRiverBuffer = new AudioSampleBuffer();
				m_pRiverBuffer->setSize(reader->numChannels, reader->lengthInSamples);
				reader->read(m_pRiverBuffer.get(),
					0,
					reader->lengthInSamples,
					0,
					true,
					true);
			}
		}
		
		setAudioChannels(0, 2);
	}


	void GenHeightMapClicked()
	{
		const int nDimension = 400;
		float waterplane = 0.3333f;		

		m_tHeights = new HeightField(nDimension);
		
		//m_tHeights->FillRandom(waterplane);
		//for (int i = 0; i < 3; ++i)
		//{
		//	m_tHeights->Erode();
		//}

		m_tHeights->FillAverage(0.5f);
		m_tHeights->m_pHeightField[ (1 + nDimension) * nDimension / 2] = 0.0001f;		

		ScopedPointer<Image> tHeightMap = new Image(Image::PixelFormat::RGB, nDimension, nDimension, false);
				
		float* pCurrentVoxel = m_tHeights->m_pHeightField;
		for (int h = 0; h < nDimension; ++h)
		{
			for (int w = 0; w < nDimension; ++w, ++pCurrentVoxel)
			{
				const float height = *pCurrentVoxel;
					
				if (height > waterplane)
				{
					tHeightMap->setPixelAt(w, h, Colour(
						(uint8)(128.f * height),
						(uint8)(256.f * height),
						0));
				}
				else
				{
					tHeightMap->setPixelAt(w, h, Colour(
						0,
						0,						
						100 + (uint8)(156.f * std::max<float>(0.f, height)/waterplane)));
				}
			}
		}

		m_tHeightMap = tHeightMap.release();
		setSize(801, 600);
	}

	TextButton guiPlayButton;
	TextButton guiGenHeightMap;	

	juce::Label guiLeftSP;
	juce::Label guiRightSP;
	juce::Label guiPower;
	juce::Label guiFilterCutoff;
	float m_filterCutoff = 0.f;
    float m_humidity = 60.f;
    bool m_bUpdateAbsorption = true;
    Butterworth1Pole m_bwFilter;
    double m_sampleRate;

    Slider guiHumiditySlider;
    juce::Label guiHumidityLabel;
    Slider guiTempatureSlider;
    juce::Label guiTempatureLabel;
    Slider guiAtmosphericPressureSlider;
    juce::Label guiAtmosphericPressureLabel;

	class HeightFieldImageComponent : public ImageComponent
	{
	public:
		HeightFieldImageComponent() 
		{
			m_bezierPath.push_back(juce::Vector3D<float>(74.f, 100.f, 1.3f));
			m_bezierPath.push_back(juce::Vector3D<float>(62.f, 88.f, 0.f));
			m_bezierPath.push_back(juce::Vector3D<float>(136.f, 48.f, 0.f));
			m_bezierPath.push_back(juce::Vector3D<float>(139.f, 69.f, 8.f)); // 3
			
			m_bezierPath.push_back(juce::Vector3D<float>(171.f, 127.f, 0.f));
			m_bezierPath.push_back(juce::Vector3D<float>(276.f, 159.f, 5.f));
			m_bezierPath.push_back(juce::Vector3D<float>(195.f, 155.f, 9.94f)); // 6

			m_bezierPath.push_back(juce::Vector3D<float>(185.f, 155.f, 23.f));
			m_bezierPath.push_back(juce::Vector3D<float>(185.f, 205.f, 0.333f));
			m_bezierPath.push_back(juce::Vector3D<float>(233.f, 205.f, 0.f)); // 9

			m_bezierPath.push_back(juce::Vector3D<float>(237.f, 215.f, -0.5f));
			m_bezierPath.push_back(juce::Vector3D<float>(45.f, 255.f, 3.333f));
			m_bezierPath.push_back(juce::Vector3D<float>(65.f, 215.f, 3.f)); // 12

			m_bezierPath.push_back(juce::Vector3D<float>(65.f, 215.f, 3.5f));
			m_bezierPath.push_back(juce::Vector3D<float>(207.f, 98.f, -2.5f));
			m_bezierPath.push_back(juce::Vector3D<float>(74.f, 100.f, 1.3f)); // 15

			SetCoefficients2(0);
			SetCoefficients2(3);
			SetCoefficients2(6);
			SetCoefficients2(9);
			SetCoefficients2(12);

			s_invTolerance = log2(s_invTolerance);
			m_intervalStorage.reserve(s_invTolerance * 2);

            m_pBezierPath = new CubicSplineTest::CubicBezierPath(
                                static_cast<const CubicSplineTest::WorldSpace*>(static_cast<void*>(&m_bezierPath[0])),
                                (int)m_bezierPath.size());

            m_pSolver = new CubicSplineTest::ClosestPointSolver();
		}

		typedef std::vector<float> tPolynomial;


		struct tSturmInterval
		{
			tSturmInterval()
			{}

			tSturmInterval(float _min, float _max, float _id, float _roots)
				: min_(_min), max_(_max), id(_id), expectedRoots(_roots)
			{}

			float min_;
			float max_;
			int id;
			int expectedRoots;
		};

		template <typename T, size_t S>
		struct tPolynomial2
		{
            alignas(16) std::array<T, S+1> m_equation;

			int size() const { return S + 1; }            

			T Evaluate(T t) const
			{						
				T result = 0.f;

				for (int i = 0; i < S; ++i)
				{
					result += m_equation[i];
					result *= t;
				}

				result += m_equation[S];

				return result;
			}
			
            //__declspec(noinline) T Evaluate5NoSimd(T t) const
            //{
            //    __m128 c = _mm_load_ps(&m_equation[0]);
            //    __m128 mul = _mm_set1_ps(t);
            //    __m128 dst = _mm_mul_ps(c, mul);
            //    mul = _mm_setr_ps(t, t, t, 1.f);
            //    dst = _mm_mul_ps(dst, mul);
            //    mul = _mm_setr_ps(t, t, 1.f, 1.f);
            //    dst = _mm_mul_ps(dst, mul);
            //    mul = _mm_setr_ps(t, 1.f, 1.f, 1.f);

            //    dst = _mm_dp_ps(dst, mul, 241);

            //    float result = _mm_cvtss_f32(dst);

            //    result += m_equation[4];
            //    result *= t;
            //    result += m_equation[5];

            //    return (T)result;
            //}

            __declspec(noinline) T Evaluate5NoSimd(T t) const
            {
                __m128 dst = _mm_set1_ps(t);
                __m128 mul = _mm_setr_ps(t, t, t, 1.f);
                dst = _mm_mul_ps(dst, mul); // 2, 2, 2, 1
                const float t2 = _mm_cvtss_f32(dst);
                mul = _mm_setr_ps(t2, t, 1.f, 1.f);
                dst = _mm_mul_ps(dst, mul); // 4, 3, 2, 1
                __m128 c = _mm_load_ps(&m_equation[0]);
                dst = _mm_dp_ps(c, dst, 241);
                dst = _mm_setr_ps(_mm_cvtss_f32(dst), m_equation[4], m_equation[5], 0.f);
                mul = _mm_setr_ps(t, t, 1.f, 0.f);
                dst = _mm_dp_ps(dst, mul, 113);

                float result = _mm_cvtss_f32(dst);

                return (T)result;
            }

            T Evaluate5NormedSimd(T t) const
            {                
                __m128 mul = _mm_set1_ps(t);
                __m128 c = _mm_setr_ps(t, m_equation[1], m_equation[2], m_equation[3]);
                __m128 dst = _mm_mul_ps(c, mul); // 2, 1, 1, 1
                const float t2 = _mm_cvtss_f32(dst);
                mul = _mm_setr_ps(t2, t2, t, 1.f);
                dst = _mm_dp_ps(dst, mul, 241); // 4, 3, 2, 1 
                dst = _mm_setr_ps(_mm_cvtss_f32(dst), m_equation[4], m_equation[5], 0.f);
                mul = _mm_setr_ps(t, t, 1.f, 0.f);
                dst = _mm_dp_ps(dst, mul, 113);

                float result = _mm_cvtss_f32(dst);
                
                return (T)result;
            }

			T Evaluate5(T t) const
			{
                return Evaluate5NormedSimd(t);
                //if (!bUseSIMD)
                //{
                //    return Evaluate5NoSimd(t);
                //}
                //else
                //{
                //    return Evaluate5NormedSimd(t);
                //}
			}

			static T Evaluate3(const T* p, T t)
			{
                __m128 c = _mm_loadu_ps(&p[0]);
				__m128 mul = _mm_setr_ps(t, t, t, 1.f);
				__m128 dst = _mm_mul_ps(c, mul);
				mul = _mm_setr_ps(t, t, 1.f, 1.f);
				dst = _mm_mul_ps(dst, mul);
				mul = _mm_setr_ps(t, 1.f, 1.f, 1.f);

				dst = _mm_dp_ps(dst, mul, 241);

				float result = _mm_cvtss_f32(dst);
				return (T)result;
			}

			static T Evaluate4(const T* p, T t)
			{
				__m128 c = _mm_loadu_ps(&p[0]);
				__m128 mul = _mm_set1_ps(t);
				__m128 dst = _mm_mul_ps(c, mul);
				mul = _mm_setr_ps(t, t, t, 1.f);
				dst = _mm_mul_ps(dst, mul);
				mul = _mm_setr_ps(t, t, 1.f, 1.f);
				dst = _mm_mul_ps(dst, mul);
				mul = _mm_setr_ps(t, 1.f, 1.f, 1.f);

				dst = _mm_dp_ps(dst, mul, 241);

				float result = _mm_cvtss_f32(dst);
				result += p[4];

				return (T)result;
			}

			static T Evaluate5(const T* p, T t)
			{
                __m128 c = _mm_loadu_ps(&p[0]);
                __m128 mul = _mm_set1_ps(t);
                __m128 dst = _mm_mul_ps(c, mul);
                mul = _mm_setr_ps(t, t, t, 1.f);
                dst = _mm_mul_ps(dst, mul);
                mul = _mm_setr_ps(t, t, 1.f, 1.f);
                dst = _mm_mul_ps(dst, mul);
                mul = _mm_setr_ps(t, 1.f, 1.f, 1.f);
                dst = _mm_dp_ps(dst, mul, 241);
                dst = _mm_setr_ps(_mm_cvtss_f32(dst), p[4], p[5], 1.f);
                mul = _mm_setr_ps(t, t, 1.f, 1.f);
                dst = _mm_dp_ps(dst, mul, 113);
                
                float result = _mm_cvtss_f32(dst);

				return (T)result;
			}

			static T Evaluate(const T* p, int degree, T t)
			{
				T dt = t;
				T result = 0.f;
				const T* _p = p;

				for (int i = 0; i < degree; ++i, ++_p)
				{
					result += *_p;
					result *= dt;
				}

				result += *_p;

				return result;
			}
		};        

		typedef juce::Vector3D<float> tCurveSpace;
		typedef juce::Vector3D<float> tWorldSpace;

		tCurveSpace CM(const tCurveSpace& rhs, const tCurveSpace& lhs)
		{
			return tCurveSpace(rhs.x * lhs.x, rhs.y * lhs.y, rhs.z * lhs.z);
		}

		void SetCoefficients2(int controlPointIndex)
		{
			juce::Vector3D<float>& p0 = m_bezierPath[controlPointIndex];
			juce::Vector3D<float>& p1 = m_bezierPath[controlPointIndex + 1];
			juce::Vector3D<float>& p2 = m_bezierPath[controlPointIndex + 2];
			juce::Vector3D<float>& p3 = m_bezierPath[controlPointIndex + 3];

			juce::Vector3D<float> n = p0*-1.f + p1*3.f - p2*3.f + p3;
			juce::Vector3D<float> r = p0*3.f - p1*6.f + p2*3.f;
			juce::Vector3D<float> s = p0*-3.f + p1*3.f;
			juce::Vector3D<float>& v = p0;

			m_bezierCurves2.push_back(tBezierCurve2());
			tBezierCurve2& curve = m_bezierCurves2.back();

			curve.m_bezierPolynomial[0] = n;
			curve.m_bezierPolynomial[1] = r;
			curve.m_bezierPolynomial[2] = s;
			curve.m_bezierPolynomial[3] = v;

			juce::Vector3D<float> j = n * 3.f;
			juce::Vector3D<float> k = r * 2.f;
			juce::Vector3D<float>& m = s;

			curve.m_derivative[0] = j;
			curve.m_derivative[1] = k;
			curve.m_derivative[2] = m;

			curve.m_bezierDotDerivative[0] = CM(j, n);
			curve.m_bezierDotDerivative[1] = CM(j, r) + CM(k, n);
			curve.m_bezierDotDerivative[2] = CM(j, s) + CM(k, r) + CM(m, n);
			curve.m_bezierDotDerivative[3] = CM(j, v) + CM(k, s) + CM(m, r);
			curve.m_bezierDotDerivative[4] = CM(k, v) + CM(m, s);
			curve.m_bezierDotDerivative[5] = CM(m, v);

			for (int i = 0; i < 6; ++i)
			{
				curve.m_independentPart.m_equation[i] = -1.f*(curve.m_bezierDotDerivative[i].x +
															  curve.m_bezierDotDerivative[i].y +
										                      curve.m_bezierDotDerivative[i].z);
				if (i == 0)
				{
					curve.m_invLeadingCoefficient = 1.f / (curve.m_independentPart.m_equation[0]);
					curve.m_independentPart.m_equation[i] = 1.0f;
				}
				else if (i < 3)
				{
					curve.m_independentPart.m_equation[i] *= curve.m_invLeadingCoefficient;
				}
			}			
		}

		bool SetPosition(int x, int y)
		{
			if (m_x == x && m_y == y)
			{
				return false;
			}

			m_x = x;
			m_y = y;

			juce::Vector3D<float> worldSpace((float)m_x, (float)m_y, 1.f);			

			tCurveSpace curve = ClosestPointToPath2(worldSpace);

			m_curveX = (int)curve.x;
			m_curveY = (int)curve.y;
			m_power = 1/((worldSpace - curve).length() + 0.0125f);

			return true;
		}		
        
		tCurveSpace ClosestPointToPath2(const tWorldSpace& worldSpace)
		{
			const std::array<tCurveSpace,5> solutions = {
				ClosestPointToCurve2(worldSpace, 0),
				ClosestPointToCurve2(worldSpace, 3),
				ClosestPointToCurve2(worldSpace, 6),
				ClosestPointToCurve2(worldSpace, 9),
				ClosestPointToCurve2(worldSpace, 12) };

			tCurveSpace closest(0.f, 0.f, 0.f);
			float closestSqDist = 1000.f * 1000.f;

			for (const tCurveSpace& point : solutions)
			{
				float distSq = (worldSpace - point).lengthSquared();
				if (distSq < closestSqDist)
				{
					closest = point;
					closestSqDist = distSq;
				}
			}

			return closest;
		}
        
		tCurveSpace ClosestPointToCurve2(const tCurveSpace& worldSpace, int controlPointIndex)
		{
			tPolynomial2<float, 5> curvePoly;
			auto& equation = curvePoly.m_equation;
			
			tBezierCurve2& bezierCurve = m_bezierCurves2[controlPointIndex / 3];
			
			for (int i = 0; i < 6; ++i)
			{
				equation[i] = bezierCurve.m_independentPart.m_equation[i];
			}

			for (int i = 0; i < 3; ++i)
			{
				equation[3 + i] += worldSpace * bezierCurve.m_derivative[i]; // dot
				equation[3 + i] *= bezierCurve.m_invLeadingCoefficient;
			}
			
			std::array<float, 5> realRoots;
			const int roots = FindRootsSturm2<5>(curvePoly, realRoots, s_tolerance, 1.f - s_tolerance);

			tCurveSpace result = m_bezierPath[controlPointIndex];
			float closest = (worldSpace - m_bezierPath[controlPointIndex]).lengthSquared();

			int controlPointLast = controlPointIndex + 3;
			std::array<tCurveSpace, 6> points = { m_bezierPath[controlPointLast] };

			for (int i = 0; i < roots; ++i)
			{
				points[i + 1] = Bezier(realRoots[i], &m_bezierPath[controlPointIndex]);
			}

			for (int i = 0; i < roots + 1; ++i)
			{
				tCurveSpace& point = points[i];
				float distSq = (worldSpace - point).lengthSquared();

				if (distSq < closest)
				{
					closest = distSq;
					result = point;
				}
			}

			return result;
		}
                
		static constexpr int ArithmeticSum(int n) { return n * (1 + n) / 2; }
		long int m_saved = 0;
		long int m_total = 0;

        template<size_t S>
        __declspec(noinline) void BuildSturm5Old(const tPolynomial2<float, S>& polynomial, float* sturmSeq)
        {
            std::array<double, ArithmeticSum(S + 1)> sturm2d;
            const int degree = S;
            std::array<int, S + 1> lookUp;

            // Build Sequence
            int lookUpIndx = 0;
            for (int i = 0; i <= S; ++i)
            {
                sturmSeq[i] = polynomial.m_equation[i];
                sturmSeq[S + 1 + i] = (S - i) * sturmSeq[i]; // derivative
                sturm2d[i] = polynomial.m_equation[i];
                sturm2d[S + 1 + i] = (S - i) * (double)sturmSeq[i]; // derivative
                lookUp[i] = lookUpIndx;
                lookUpIndx += S - i + 1;
            }

            for (int i = 2; i <= degree; ++i)
            {
                const double* p2 = &sturm2d[lookUp[i - 2]];
                const double* p1 = &sturm2d[lookUp[i - 1]];

                float* element = &sturmSeq[lookUp[i]];
                double* elementd = &sturm2d[lookUp[i]];

                const double remCoefficeint = (double)p2[0] / p1[0];
                const double remSum = (p2[1] - remCoefficeint * p1[1]) / p1[0];
                for (int c = 0; c <= degree - i; ++c)
                {
                    double coefficient = remSum * p1[c + 1] - p2[c + 2];
                    if (c != degree - i)
                    {
                        coefficient += remCoefficeint * p1[c + 2];
                    }

                    elementd[c] = coefficient;
                    element[c] = (float)coefficient;
                }
            }
        }

        template<size_t S>
        __declspec(noinline) void BuildSturm5New(const tPolynomial2<float, S>& polynomial, float* out_sturmSeq)
        {
            std::array<double, ArithmeticSum(S + 1)> sturm2d;
            const int degree = S;            

            for (int i = 0; i <= S; ++i)
            {
                sturm2d[i] = polynomial.m_equation[i];
                sturm2d[S + 1 + i] = (S - i) * sturm2d[i]; // derivative
            }

            int index = 6;
            double* elementd = &sturm2d[6 + 5];
            for (int i = 2; i <= degree; ++i)
            {
                const int poly = degree - i;
                const double* p1 = &sturm2d[index];
                const double* p2 = &sturm2d[index - (poly + 3)];
                index += poly + 2;                

                const double remCoefficeint = (double)p2[0] / p1[0];
                const double remSum = (p2[1] - remCoefficeint * p1[1]) / p1[0];
                for (int c = 0; c <= poly; ++c, ++elementd)
                {
                    double coefficient = remSum * p1[c + 1] - p2[c + 2];
                    if (c != poly)
                    {
                        coefficient += remCoefficeint * p1[c + 2];
                    }

                    *elementd = coefficient;
                }
            }

            for (int i = 0; i < ArithmeticSum(S + 1); ++i)
            {
                out_sturmSeq[i] = (float)sturm2d[i];
            }
        }

        template<size_t S>
        void BuildSturm5(const tPolynomial2<float, S>& polynomial, float* sturmSeq)
        {
            if (!bUseSIMD)
            {
                BuildSturm5Old<S>(polynomial, sturmSeq);
            }
            else
            {
                BuildSturm5New<S>(polynomial, sturmSeq);
            }
        }

		template<size_t S>
		int FindRootsSturm2(const tPolynomial2<float, S>& polynomial,
			std::array<float, S>& realRoots,
			float intervalMin,
			float intervalMax)
		{									
			std::array<float, ArithmeticSum(S + 1)> sturm2;
            BuildSturm5<S>(polynomial, &sturm2[0]);

			m_intervalStorage.clear();
			int id = 0;
			m_intervalStorage.emplace_back(intervalMin, intervalMax, id, 0);
			std::array<tSturmInterval, S> roots;
			int allRoots = 0;
			int totalRoots = 1;
			
			while (!m_intervalStorage.empty() && totalRoots != allRoots)
			{
				tSturmInterval i = m_intervalStorage.back();
				m_intervalStorage.pop_back();

				int nRoots = CountRealRoots2<S>(&sturm2[0], i.min_, i.max_);

				if (id == 0) // initialize
				{
					totalRoots = nRoots;
                    ++id;
				}

				if (nRoots <= 0)
				{		
					if (!m_intervalStorage.empty() &&
						m_intervalStorage.back().id == i.id)
					{
						i = m_intervalStorage.back();
						m_intervalStorage.pop_back();
						nRoots = i.expectedRoots;
					}
					else
					{
						continue;
					}					
				}
				
				if (nRoots == i.expectedRoots &&
					!m_intervalStorage.empty() &&
					m_intervalStorage.back().id == i.id)
				{
					m_intervalStorage.pop_back();
				}
				else if (nRoots == i.expectedRoots - 1 &&
					!m_intervalStorage.empty() &&
					m_intervalStorage.back().id == i.id) // this is biggest perf
				{
					roots[allRoots++] = m_intervalStorage.back();
					m_intervalStorage.pop_back();
				}

				if (nRoots == 1)
				{
					roots[allRoots++] = i;
				}
				else
				{
					float mid = (i.min_ + i.max_) / 2.f;
					if (mid - i.min_ <= s_tolerance)
					{
						roots[allRoots++] = i;
					}
					else
					{
						m_intervalStorage.emplace_back(i.min_, mid, id, nRoots);
						m_intervalStorage.emplace_back(mid, i.max_, id, nRoots);						
						++id;
					}
				}
			}

			int numRealRoots = 0;
			for (int i = 0; i < allRoots; ++ i)
			{
				const tSturmInterval& interval = roots[i];
				float root = SolveBisection2<5>(polynomial, interval.min_, interval.max_);
				if (!isnan(root))
				{
					realRoots[numRealRoots++] = root;
				}
			}

			return numRealRoots;
		}

		template<size_t S>
		float SolveBisection2(const tPolynomial2<float, S>& p, float intervalMin, float intervalMax)
		{			
			if (p.Evaluate5(intervalMin) > 0)
			{
				return NAN;
			}
			if (p.Evaluate5(intervalMax) < 0)
			{
				return NAN;
			}

			const int maxIterations = 1 + (uint32)s_invTolerance;
			for (uint32 i = 0; i < maxIterations; ++i)
			{
				const float mid = (intervalMax + intervalMin) / 2.f;

				const float r = p.Evaluate5(mid);
				const union { float flVal; uint32 nVal; } rSign = { r };				
				if (rSign.nVal & 0x80000000u)
				{
					if (r >= -s_tolerance)
					{
						return mid;
					}
					intervalMin = mid;
				}
				else
				{
					if (r <= s_tolerance)
					{
						return mid;
					}
					intervalMax = mid;
				}
			}

			return intervalMin;
		}

        template<size_t S>
        __declspec(noinline) int CountRealRootsNoSimd(const float* sturmSeq, float intervalMin, float intervalMax)
        {
            int signChange = 0;
            int index = ArithmeticSum(S + 1) - 1;
            const union { float flVal; uint32 nVal; } lastSign = { sturmSeq[index] };
            uint32 previousMin = lastSign.nVal;
            uint32 previousMax = previousMin;
            float sMin = 0.f, sMax = 0.f;
            for (int i = 1; i <= S; ++i)
            {
                index -= i + 1;
                const float* seq = &sturmSeq[index];                
                sMin = tPolynomial2<float, S>::Evaluate(seq, i, intervalMin);
                sMax = tPolynomial2<float, S>::Evaluate(seq, i, intervalMax);                

                union { float flVal; uint32 nVal; } sSignMin = { sMin };
                signChange += (previousMin ^ sSignMin.nVal) >> 31;
                previousMin = sSignMin.nVal;

                union { float flVal; uint32 nVal; } sSignMax = { sMax };
                signChange -= (previousMax ^ sSignMax.nVal) >> 31;
                previousMax = sSignMax.nVal;
            }

            return signChange;
        }

        int CountSturmSignChanges(const float* sturmSeq, float t)
        {
            const float *p5 = &sturmSeq[0];
            const float *p4 = &sturmSeq[6];
            const float *p3 = &sturmSeq[6 + 5];
            const float *p2 = &sturmSeq[6 + 5 + 4];

            __m128 c = _mm_setr_ps(*p5, *p4, *p3, *p2);
            __m128 mul = _mm_set1_ps(t);
            __m128 dst = _mm_mul_ps(c, mul); // 1
            c = _mm_setr_ps(*(++p5), *(++p4), *(++p3), *(++p2));
            dst = _mm_add_ps(c, dst);
            dst = _mm_mul_ps(dst, mul); // 2
            c = _mm_setr_ps(*(++p5), *(++p4), *(++p3), *(++p2));
            dst = _mm_add_ps(c, dst);
            mul = _mm_setr_ps(t, t, t, 1.f);
            dst = _mm_mul_ps(dst, mul); // 3
            c = _mm_setr_ps(*(++p5), *(++p4), *(++p3), 0.f);
            dst = _mm_add_ps(c, dst);
            mul = _mm_setr_ps(t, t, 1.f, 1.f);
            dst = _mm_mul_ps(dst, mul); // 4
            c = _mm_setr_ps(*(++p5), *(++p4), 0.f, 0.f);
            dst = _mm_add_ps(c, dst);
            mul = _mm_setr_ps(t, 1.f, 1.f, 1.f);
            dst = _mm_mul_ps(dst, mul); // 5
            alignas(16) float sturmResult[5];
            _mm_store_ps(&sturmResult[0], dst);
            sturmResult[0] += *(++p5);
            sturmResult[4] = sturmSeq[6 + 5 + 4 + 3] * t + sturmSeq[6 + 5 + 4 + 3 + 1];

            int signChanges = 0;
            union { float flVal; uint32 nVal; } sign = { sturmSeq[ArithmeticSum(5 + 1) - 1] };
            uint32 previous = sign.nVal;
            for (int i = 4; i >= 0; --i)
            {
                sign.flVal = sturmResult[i];
                signChanges += (previous ^ sign.nVal) >> 31;
                previous = sign.nVal;
            }

            return signChanges;
        }

        template<size_t S>
        __declspec(noinline) int CountRealRootsSimd(const float* sturmSeq, float intervalMin, float intervalMax)
        {
            int numRoots = CountSturmSignChanges(sturmSeq, intervalMin) - CountSturmSignChanges(sturmSeq, intervalMax);
            return numRoots;
        }

		template<size_t S>
		int CountRealRoots2(const float* sturmSeq, float intervalMin, float intervalMax)
		{
            if (bUseSIMD)
            {
                return CountRealRootsSimd<S>(sturmSeq, intervalMin, intervalMax);
            }
            else
            {
                return CountRealRootsNoSimd<S>(sturmSeq, intervalMin, intervalMax);
            }
		}

		void paint(Graphics& g) override
		{
			ImageComponent::paint(g);

            bool bProfileLoudness = false;
            if (!bProfileLoudness)
            {
                paintBezier(g);

                if (m_x >= 0 && m_y >= 0)
                {
                    g.setColour(Colour(255, 200, 96));
                    g.drawEllipse(m_x, m_y, 5, 5, 1);
                }

                g.setColour(Colour(0, 200, 200));
                for (auto var : m_bezierPath)
                {
                    g.fillRect((int)var.x, (int)var.y, 2, 2);
                }

                if (m_curveX >= 0 && m_curveX >= 0)
                {
                    g.setColour(Colour(96, 96, 96));
                    g.fillEllipse(m_curveX, m_curveY, 4, 4);
                }
            }
		}

		void paintBezier(Graphics& g)
		{
			const int numSegments = 16;
			int controlPoints = m_bezierPath.size();
			
			g.setColour(Colour(200, 200, 0));
			for (int i = 0; i < controlPoints - 3; i += 3)
			{
				Vector3D<float> start = Bezier(0.f, &m_bezierPath[i]);
				for (int segment = 1; segment <= numSegments; ++segment)
				{
					float t = (float)segment / numSegments;
					Vector3D<float> end = Bezier(t, &m_bezierPath[i]);
					g.drawLine(start.x, start.y, end.x, end.y);
					start = end;
				}				
			}

			int nDimension = 300;

			HeightField tHeightPowers(nDimension);
			HeightField tHeightDirection(nDimension);
			
   //         bUseSIMD = false;
			//CalcCurvePowers2(tHeightPowers, tHeightDirection);
            //bUseCubicSpline = false;
            //bUseSIMD = true;
            //CalcCurvePowers2(tHeightPowers, tHeightDirection);
            //bUseCubicSpline = true;
            CalcCurvePowers2(tHeightPowers, tHeightDirection);

            //LARGE_INTEGER li;
            //QueryPerformanceFrequency(&li);
            //double scaleToMS = 1000.0 / li.QuadPart;

            //LARGE_INTEGER liStart;
            //QueryPerformanceCounter(&liStart);

            //float q = ProfileAlgorithm();

            //LARGE_INTEGER liEnd;
            //QueryPerformanceCounter(&liEnd);

            //double tElapsed = (liEnd.QuadPart - liStart.QuadPart) * scaleToMS;
            //char buf[200];
            //sprintf_s(buf, "Elapsed: %.4f\n", (float)tElapsed);
            //OutputDebugString(buf);

            //q += (liEnd.QuadPart - liStart.QuadPart) * scaleToMS;

            //liStart;
            //QueryPerformanceCounter(&liStart);

            //q += ProfileAlgorithmDoNothing();

            //liEnd;
            //QueryPerformanceCounter(&liEnd);

            //tElapsed = (liEnd.QuadPart - liStart.QuadPart) * scaleToMS;
            //sprintf_s(buf, "Elapsed DN: %.4f\n", (float)tElapsed);
            //OutputDebugString(buf);
            //
            //if (q < 0)
            //    return;

			for (int r = 0; r < nDimension; ++r)
			{
				for (int c = 0; c < nDimension; ++c)
				{
					float brightness = jmin(1.f, tHeightPowers.GetValue(c, r));
					int color = 255.f * brightness;
					const union { float flVal; uint32 nVal; } hack = { tHeightDirection.GetValue(c, r) };

					int red = (int)((hack.nVal >> 16) * sqrt(brightness));
					int blue = (int)((hack.nVal & 0xFF) * sqrt(brightness));

					//g.setColour(juce::Colour(red, 0, blue));
					g.setColour(juce::Colour(color, color, color));
					g.setPixel(c, r);
				}
			}

			return;
		}

        _declspec(noinline) float ProfileAlgorithmDoNothing()
        {
            CubicSplineTest::WorldSpace total = { 0.f, 0.f, 0.f };
#ifdef PROFILE_ENABLE
            BROFILER_CATEGORY("DoNothing", Profiler::Color::YellowGreen);
#endif // PROFILE_ENABLE
            for (int r = 0; r < 300; ++r)
            {
                for (int c = 0; c < 300; ++c)
                {
                    const CubicSplineTest::WorldSpace solution{ (float)c, (float)r, 1.f };                    
                    total.x += solution.x;
                    total.y += solution.y;
                    total.z += solution.z;
                }
            }

            return total.x + total.y + total.z;
        }

        _declspec(noinline) float ProfileAlgorithm()
        {
            CubicSplineTest::WorldSpace total = { 0.f, 0.f, 0.f };
#ifdef PROFILE_ENABLE
            BROFILER_CATEGORY("DontCrashOnMe", Profiler::Color::RoyalBlue);
#endif // PROFILE_ENABLE
            for (int r = 0; r < 150; ++r)
            {
                for (int c = 0; c < 150; ++c)
                {
                    const CubicSplineTest::WorldSpace position{ (float)c, (float)r, 1.f };
                    CubicSplineTest::WorldSpace solution = m_pBezierPath->ClosestPointToPath(position, m_pSolver.get());
                    total.x += solution.x;
                    total.y += solution.y;
                    total.z += solution.z;
                }
            }

            return total.x + total.y + total.z;
        }

		//float listenerHeight = -30.0f;
        float listenerHeight = 1.f;
		_declspec(noinline) void CalcCurvePowers2(HeightField& tHeightPowers, HeightField& tHeightDirection)
		{
#ifdef PROFILE_ENABLE
			BROFILER_CATEGORY("ClosestPoint2", Profiler::Color::Bisque);
#endif // PROFILE_ENABLE

			int nDimension = tHeightPowers.m_nDimension;
			for (int r = 0; r < nDimension; ++r)
			{
				for (int c = 0; c < nDimension; ++c)
				{
					const tCurveSpace worldSpace = juce::Vector3D<float>((float)c, (float)r, listenerHeight);
                    tCurveSpace curve;
                    if (bUseCubicSpline)
                    {
                        const CubicSplineTest::WorldSpace position { worldSpace.x, worldSpace.y, worldSpace.z };
                        CubicSplineTest::WorldSpace solution = m_pBezierPath->ClosestPointToPath(position, m_pSolver.get());
                        curve.x = solution.x;
                        curve.y = solution.y;
                        curve.z = solution.z;
                    }
                    else
                    {
                        curve = ClosestPointToPath2(worldSpace);
                    }

					float nearField = 0.0125f;
					float distance = (curve - worldSpace).length();
					float power = jmin(1.f, sqrt(1 / (distance + nearField)));

					float dy = curve.y - worldSpace.y;
					float dySq = dy*dy;
					double dotHalf = dySq != 0 ? 0.5 * abs((double)dy) / distance : 0.0; // + 0 * dx

					if (distance < s_tolerance)
					{
						dotHalf = 0.5;
					}

					uint32 rgb = 0;
					if (worldSpace.x < curve.x)
					{
						rgb = ((uint32)(255.f * dotHalf) << 16) | (uint32)(255.f * (1.f - dotHalf));
					}
					else
					{
						rgb = ((uint32)(255.f * (1.f - dotHalf)) << 16) | (uint32)(255.f * dotHalf);
					}
					const union { uint32 nVal; float flVal; } hack = { rgb };

					tHeightPowers.SetValue(c, r, power);
					tHeightDirection.SetValue(c, r, hack.flVal);
				}
			}
			//listenerHeight += 0.333f;
			//if (listenerHeight > 30.f) listenerHeight = -30.f;
		}

        _declspec(noinline) juce::Vector3D<float> Bezier(float t, const Vector3D<float>* ptControlPoints)
		{
			const float u = 1.f - t;

			Vector3D<float> position = ptControlPoints[0] * (u * u * u);
			position += ptControlPoints[1] * (3 * u * u * t);
			position += ptControlPoints[2] * (3 * u * t * t);
			position += ptControlPoints[3] * (t * t * t);

			return position;
		}

		float m_power = 0;
	private:
		int m_x, m_y = -1;
		int m_curveX, m_curveY = -1;
		
		std::vector<juce::Vector3D<float>> m_bezierPath;

        ScopedPointer<CubicSplineTest::CubicBezierPath> m_pBezierPath;
        ScopedPointer<const CubicSplineTest::ClosestPointSolver> m_pSolver;

		std::vector<tSturmInterval> m_intervalStorage;

		struct tBezierCurve2
		{
			juce::Vector3D<float> m_derivative[3];
			juce::Vector3D<float> m_bezierPolynomial[4];
			juce::Vector3D<float> m_bezierDotDerivative[6];

			tPolynomial2<float, 5> m_independentPart;
			float m_invLeadingCoefficient;
		};

		std::vector<tBezierCurve2> m_bezierCurves2;
		const float s_tolerance = 0.00001f;
		float s_invTolerance = 1.f/0.00001f;
	};	

	HeightFieldImageComponent guiHeightFieldComponent;

	AudioFormatManager formatManager;	
	//ScopedPointer<AudioFormatReaderSource> readerSource;
	//AudioTransportSource transportSource;

	enum SampleBufferType
	{
		SBT_None,
		SBT_Ocean,
		SBT_Pond,
		SBT_River
	};

	ScopedPointer<AudioSampleBuffer> m_pOceanBuffer;
	ScopedPointer<AudioSampleBuffer> m_pPondBuffer;
	ScopedPointer<AudioSampleBuffer> m_pRiverBuffer;

	struct Audio3DSampleSource
	{
		Audio3DSampleSource(SampleBufferType bufferType)
			: m_bPlaying(0)
			, m_eBufferType(bufferType)
		{}

		Audio3DSampleSource()
			: m_bPlaying(0)
			, m_eBufferType(SBT_None)
		{}

		SampleBufferType m_eBufferType;
		int m_nBufferIndex = 0;		
		float m_flGainL = 1.f;
		float m_flGainR = 1.f;
		Atomic<int> m_bPlaying;
	};

	#define MAX_AUDIO_VOICES 64

	Audio3DSampleSource m_pSources[MAX_AUDIO_VOICES];
	int nNumSources = 0;

	Audio3DSampleSource* AddAudioSource(SampleBufferType bufferType)
	{
		Audio3DSampleSource* pSource = new(&m_pSources[nNumSources]) Audio3DSampleSource(bufferType);
		++nNumSources;

		return pSource;
	}

	ScopedPointer<HeightField> m_tHeights;
	ScopedPointer<Image> m_tHeightMap;

	Atomic<int> m_ListenerPosX;
	Atomic<int> m_ListenerPosY;	

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED

#include "brutefilter.hpp"

Plugin *plugin;

void init(Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	// Add all Models defined throughout the plugin
	p->addModel(modelBruteFilter);

	
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}



struct BruteFilter : Module {
	enum ParamIds {
		CUTOFF_PARAM,
		CUTOFF_AMOUNT_PARAM,
		RESONANCE_PARAM,
		RESONANCE_AMOUNT_PARAM,
		ATT_AMOUNT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AUDIO_DATA_INPUT,
		CUTOFF_INPUT,
		RESONANCE_INPUT,
		ATT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		HIGHPASS_OUTPUT,
		LOWPASS_OUTPUT,
		BANDPASS_OUTPUT,
		NOTCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};


	RCFilter firstFilter;
	RCFilter secondFilter;
	RCFilter highpassFilter;
	
	PeakFilter lowpassResonance;
	PeakFilter highpassResonance;
	PeakFilter bandpassResonance;
	PeakFilter notchResonance;

	float resonance = 0.0;
	float resonance_amount = 0.0;
	float cutoff_amount = 0.0;
	float att_amount = 0.0;
	float maxCutoffFreq = 20000.0f;
	

	float firstLowpass;
	float firstHighpass;
	float secondLowpass;
	float secondHighpass;
	float bandpass;
	float notch;
	float dampingFactor = 2.5f;

	float buffer;

	BruteFilter() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	//TODO Upsampler code


	//moduationCutoff takes the Cutoff_input. scales it by 
	float ModulateCutoff()
	{
		float cutoffAmount = params[CUTOFF_AMOUNT_PARAM].value;
		float cutoffCV = clamp(inputs[CUTOFF_INPUT].value, -1.f, 1.f);
		float cutoffFrequency = 20 * powf(10, params[CUTOFF_PARAM].value);
		float cutoffModulation = cutoffAmount * cutoffCV;
		float ATT = 1.f;

		// if there is an ATT input connectect, the ATT value input * amount, otherwise ATT = 1
		if (inputs[ATT_INPUT].active)
		{
			ATT = params[ATT_AMOUNT_PARAM].value * clamp(inputs[ATT_INPUT].value, 0.f, 1.f);
		}
		
		cutoffModulation = cutoffModulation * ATT;
		
		//There is a glitch that when the modulation = 0, the audio breaks, outputting the frequency fixes it
		if (cutoffModulation > 0.0f)
		{
			cutoffModulation = cutoffModulation * -1.0f;
			cutoffModulation = cutoffModulation * cutoffFrequency;
			cutoffModulation = cutoffFrequency - cutoffModulation;
		}
		else if (cutoffModulation < 0.0f)
		{
			cutoffModulation = cutoffModulation * cutoffFrequency;
			cutoffModulation = cutoffFrequency + cutoffModulation;
		}
		else
		{
			return cutoffFrequency;
		}
		
		//return cutoffFrequency;
		return clamp(cutoffModulation, 20.f, 22000.f);
	}
	

	//monduateResonance scales the resonance input by resonance amount and modulates the resonance.
	float ModulateResonance() {
		float resonanceParam = params[RESONANCE_PARAM].value;
		float resonanceAmount = params[RESONANCE_AMOUNT_PARAM].value;
		float resonanceCV = inputs[RESONANCE_INPUT].value;
		float resonanceModulation = 1.0f;

		resonanceModulation = resonanceAmount * resonanceCV;
		resonanceModulation = resonanceParam + resonanceModulation;
		resonanceModulation = resonanceModulation * dampingFactor;
		resonanceModulation = resonanceModulation * dampingFactor;

		return resonanceModulation;
	}
	

	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void BruteFilter::step() {

	float sampleRate = engineGetSampleRate();
	//use the samplerate to calulate the ratio of cutoff
	
	float cutoff = BruteFilter::ModulateCutoff();
	
	
	float cutoffRatio = cutoff / sampleRate;

	//set cutoff freqencies
	firstFilter.setCutoff(cutoffRatio);
	secondFilter.setCutoff(cutoffRatio);
	highpassFilter.setCutoff(cutoffRatio);

	//buffer generation

	buffer = inputs[AUDIO_DATA_INPUT].value;
	buffer += 0.01 * (2.f * randomUniform() - 1.f);

	//first stage of filtering
	firstFilter.process(buffer);
	firstLowpass = firstFilter.lowpass();
	firstHighpass = firstFilter.highpass();



	//second stage of filtering
	
	secondFilter.process(firstLowpass);
	secondLowpass = secondFilter.lowpass();
	bandpass = secondFilter.highpass();

	highpassFilter.process(firstHighpass);
	secondHighpass = highpassFilter.highpass();

	notch = buffer - bandpass;
	
	//resonance section
	resonance = BruteFilter::ModulateResonance();
	lowpassResonance.setRate(1/ cutoff);
	highpassResonance.setRate(1/ cutoff);
	bandpassResonance.setRate(1/ cutoff);
	notchResonance.setRate(1/ cutoff);
	
	lowpassResonance.process(resonance);
	highpassResonance.process(resonance);
	bandpassResonance.process(resonance);
	notchResonance.process(resonance);
	

	//TODO add a saturation alogorithm that would be cool!
	outputs[LOWPASS_OUTPUT].value = lowpassResonance.peak()* secondLowpass;
	outputs[HIGHPASS_OUTPUT].value = highpassResonance.peak()* secondHighpass;
	outputs[BANDPASS_OUTPUT].value = bandpassResonance.peak()* bandpass;
	outputs[NOTCH_OUTPUT].value = notchResonance.peak()* bandpass;
	
}


struct BruteFilterWidget : ModuleWidget {
	BruteFilterWidget(BruteFilter *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/BruteFilter.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(11, 87), module, BruteFilter::CUTOFF_PARAM, 0.1f, 2.9f, 2.9f));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(11, 142), module, BruteFilter::CUTOFF_AMOUNT_PARAM, -1.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(56, 87), module, BruteFilter::RESONANCE_PARAM, 0.1f, 1.f, 0.1f));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(56, 142), module, BruteFilter::RESONANCE_AMOUNT_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(101, 142), module, BruteFilter::ATT_AMOUNT_PARAM, 0.0f, 1.0f, 0.0f));
		
		addInput(Port::create<PJ301MPort>(Vec(11, 276), Port::INPUT, module, BruteFilter::AUDIO_DATA_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(45, 276), Port::INPUT, module, BruteFilter::CUTOFF_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(80, 276), Port::INPUT, module, BruteFilter::RESONANCE_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(114, 276), Port::INPUT, module, BruteFilter::ATT_INPUT));
		

		
		addOutput(Port::create<PJ301MPort>(Vec(11, 320), Port::OUTPUT, module, BruteFilter::LOWPASS_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(45, 320), Port::OUTPUT, module, BruteFilter::HIGHPASS_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(80, 320), Port::OUTPUT, module, BruteFilter::BANDPASS_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(114, 320), Port::OUTPUT, module, BruteFilter::NOTCH_OUTPUT));

	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelBruteFilter = Model::create<BruteFilter, BruteFilterWidget>("BruteFilter", "BruteFilter", "BruteFilter", FILTER_TAG);

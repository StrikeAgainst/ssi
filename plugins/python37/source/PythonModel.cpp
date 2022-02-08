// PythonConsumer.cpp
// author: Johannes Wagner <wagner@hcm-lab.de>
// created: 2016/03/02
// Copyright (C) University of Augsburg, Lab for Human Centered Multimedia
//
// *************************************************************************************************
//
// This file is part of Social Signal Interpretation (SSI) developed at the 
// Lab for Human Centered Multimedia of the University of Augsburg
//
// This library is free software; you can redistribute itand/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or any laterversion.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FORA PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along withthis library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
//*************************************************************************************************

#include "PythonModel.h"
#include "PythonHelper.h"
#include "ioput/file/FilePath.h"

#ifdef USE_SSI_LEAK_DETECTOR
	#include "SSI_LeakWatcher.h"
	#ifdef _DEBUG
		#define new DEBUG_NEW
		#undef THIS_FILE
		static char THIS_FILE[] = __FILE__;
	#endif
#endif

namespace ssi {

ssi_char_t *PythonModel::ssi_log_name = "pymodel";

PythonModel::PythonModel(const ssi_char_t *file)
	: _file (0),
	_isTrained(false),
	_helper(0),
	_has_meta_data(false)
{

	if (file) {
		if (!OptionList::LoadXML(file, &_options)) {
			OptionList::SaveXML(file, &_options);
		}
		_file = ssi_strcpy (file);
	}
}

PythonModel::~PythonModel() {

	if (_file) {
		OptionList::SaveXML(_file, &_options);
		delete[] _file;
	}
	
	delete _helper;	_helper = 0;
}

void PythonModel::initHelper()
{
	ssi_char_t *workdir = 0;
	if (_file)
	{
		FilePath fp(_file);
		workdir = ssi_strcpy(fp.getDir());
	}

	_helper = new PythonHelper(_options.script, _options.optsfile, _options.optsstr, _options.syspath, workdir);

	ssi_msg(SSI_LOG_LEVEL_DETAIL, "python is ready");

	delete[] workdir;
}


void PythonModel::release()
{

}

IModel::TYPE::List PythonModel::getModelType()
{
	if (!_helper)
	{
		initHelper();
	}

	IModel::TYPE::List type = IModel::TYPE::CLASSIFICATION;
	if (!_helper->getModelType(type))
	{
		ssi_wrn("could not get model type");
	}

	return type;
}

bool PythonModel::train(ISamples &samples,
	ssi_size_t stream_index) {
	
	if (!_helper)
	{
		initHelper();
	}

	IModel::TYPE::List type = getModelType();
	_isTrained = _helper->train(type, samples, stream_index);

	return _isTrained;
}

void PythonModel::setMetaData(ssi_size_t size, const void *meta) {

	if (sizeof(_format_in) != size)
	{
		ssi_err("meta data does not describe image format");
	}

	if (!_helper)
	{
		initHelper();
	}

	memcpy(&_format_in, meta, size);
	_has_meta_data = true;

	ssi_msg(SSI_LOG_LEVEL_BASIC, "format of input image '%dx%dx%dx%d'", _format_in.widthInPixels, _format_in.heightInPixels, _format_in.numOfChannels, _format_in.depthInBitsPerChannel / 8);

	_helper->setImageFormatIn(_format_in);
};


bool PythonModel::forward(ssi_stream_t &stream, ssi_size_t n_probs, ssi_real_t *probs, ssi_real_t &confidence) {
	


	if (!_helper)
	{
		initHelper();
	}
	 
	
	if (stream.type == SSI_IMAGE)
	{	
		//temporary fix, setMetaData gets not called by the framework..  Check for default video resolutions.
		if (stream.byte == 230400)
		{
			_format_in.widthInPixels = 320;
			_format_in.heightInPixels = 240;
		}

		else if (stream.byte == 921600)
		{
			_format_in.widthInPixels = 640;
			_format_in.heightInPixels = 480;
		}

		else if (stream.byte == 2764800)
		{
			_format_in.widthInPixels = 1280;
			_format_in.heightInPixels = 720;
		}

		else if (stream.byte == 6220800)
		{
			_format_in.widthInPixels = 1920;
			_format_in.heightInPixels = 1080;
		}

		else if (stream.byte == 11059200)
		{
			_format_in.widthInPixels = 2560;
			_format_in.heightInPixels = 1440;
		}

		else if (stream.byte == 24883200)
		{
			_format_in.widthInPixels = 3840;
			_format_in.heightInPixels = 2160;
		}

		//If not a default video resolution, expect a squared image.
		else
		{
			int square = stream.byte / 3;
			_format_in.widthInPixels = sqrt(square);
			_format_in.heightInPixels = sqrt(square);
		}

		_format_in.depthInBitsPerChannel = 8;
		_format_in.numOfChannels = 3;
		_format_in.framesPerSecond = stream.sr;

		_helper->setImageFormatIn(_format_in);
		

		return _helper->forward(stream, n_probs, probs, confidence, _format_in);
	}

	else return _helper->forward(stream, n_probs, probs, confidence);
}


bool PythonModel::forward(ssi_stream_t &stream, ssi_size_t n_probs, ssi_real_t *probs, ssi_real_t &confidence, ssi_video_params_t &params) {

	if (!_helper)
	{
		initHelper();
	}
	_helper->setImageFormatIn(params);

	return _helper->forward(stream, n_probs, probs, confidence, params);
}

bool PythonModel::load(const ssi_char_t *filepath) {
	
	if (!_helper)
	{
		initHelper();
	}

	return _helper->load(filepath);
}

bool PythonModel::save(const ssi_char_t *filepath) {
	
	if (!_helper)
	{
		initHelper();
	}

	return _helper->save(filepath);
}
}

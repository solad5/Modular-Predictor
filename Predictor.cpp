#include "Predictor.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <math.h>
using namespace std;
using namespace Predictor;

vector<float> read_1d_array(ifstream &fin, int cols)
{
	vector<float> arr;
	float tmp_float;
	for (int n = 0; n < cols; n++)
	{
		fin >> tmp_float;
		arr.push_back(tmp_float);
	}
	return arr;
}

void missing_activation_impl(const string &act) {
	cout << "Activation " << act << " not defined!" << endl;
	cout << "Please add its implementation before use." << endl;
	exit(1);
}


void DataChunk2D::read_from_file(const string &fname)
{
	ifstream fin(fname.c_str());
	fin >> m_depth >> m_length;
	for (int d = 0; d < m_depth; d++) {
		vector<float> tmp_single_depth = read_1d_array(fin, m_length);
		data.push_back(tmp_single_depth);
	}
	fin.close();
}

void DataChunk3D::read_from_file(const string &fname)
{
	ifstream fin(fname.c_str());
	fin >> m_depth >> m_row >> m_col;
	for (int d = 0; d < m_depth; d++) {
		vector<vector<float>> tmp_single_depth;
		for (int r = 0; r < m_row; r++) {
			vector<float> tmp_row = read_1d_array(fin, m_col);
			tmp_single_depth.push_back(tmp_row);
		}
		data.push_back(tmp_single_depth);
	}
	fin.close();
}

//Embedding Layer
void LayerEmbedding::load_weights(ifstream &fin) {
	fin >> m_vocab_size >> m_dimension;
	for (int i = 0; i < m_vocab_size; i++)
	{
		vector<float> tmp_emb = read_1d_array(fin, m_dimension);
		m_embs.push_back(tmp_emb);
	}
}

DataChunk* LayerEmbedding::compute_output(DataChunk* dc) {
	vector<vector<float>> tmp_embs;
	vector<float> input = dc -> get_1d();
	for (unsigned int i = 0; i < input.size(); i++)
		tmp_embs.push_back(m_embs[input[i]]);
	DataChunk *out = new DataChunk2D();
	out->set_data(tmp_embs);
	return out;
}

//Dense Layer
void LayerDense::load_weights(ifstream &fin) { // Dimensions: input * output
	fin >> m_input_cnt >> m_output_cnt;
	for (int i = 0; i < m_input_cnt; i++)
	{
		vector<float> tmp_single_row = read_1d_array(fin, m_output_cnt);
		m_weights.push_back(tmp_single_row);
	}
	m_bias = read_1d_array(fin, m_output_cnt);
}

DataChunk* LayerDense::compute_output(DataChunk* dc) {
	vector<float> y_ret(m_weights[0].size(), 0.0);
	vector<float> im = dc->get_1d();
	for (unsigned int i = 0; i < m_weights.size(); i++)  //iter over input
		for (unsigned int j = 0; j < m_weights[0].size(); j++) //iter over output
			y_ret[j] += m_weights[i][j] * im[i];
	for (unsigned int j = 0; j < m_bias.size(); j++)
		y_ret[j] += m_bias[j];
	DataChunk *out = new DataChunk1D();
	out->set_data(y_ret);
	return out;
}

//Activation Layer
DataChunk* LayerActivation::compute_output(DataChunk* dc) {
	if (dc->get_data_dim() == 3){
		vector<vector<vector<float>>> y = dc->get_3d();
		if (m_activation_type == "relu") {
			for (unsigned int i = 0; i < y.size(); i++)
				for (unsigned int j = 0; j < y[0].size(); j++)
					for (unsigned int k = 0; k < y[0][0].size(); k++)
						if (y[i][j][k] < 0) y[i][j][k] = 0;
			DataChunk *out = new DataChunk3D();
			out->set_data(y);
			return out;
		}
		else {
			missing_activation_impl(m_activation_type);
		}
	}
	else if (dc->get_data_dim() == 2) {
		vector<vector<float>> y = dc->get_2d();
		if (m_activation_type == "relu") {
			for (unsigned int i = 0; i < y.size(); i++)
				for (unsigned int j = 0; j < y[0].size(); j++)
					if (y[i][j] < 0) y[i][j] = 0;
			DataChunk *out = new DataChunk2D();
			out->set_data(y);
			return out;
		}
		else {
			missing_activation_impl(m_activation_type);
		}
	}
	else if (dc->get_data_dim() == 1) {
		vector<float> y = dc->get_1d();
		if (m_activation_type == "relu") {
			for (unsigned int i = 0; i < y.size(); i++)
				if (y[i] < 0) y[i] = 0;
			DataChunk *out = new DataChunk1D();
			out->set_data(y);
			return out;
		}
		else{
			missing_activation_impl(m_activation_type);
		}
	}
	else { throw "Data dim not supported."; }
	return dc;
}
//Conv1D


//MaxPooling : May not be right, easist version
DataChunk* LayerMaxPooling::compute_output(DataChunk* dc)
{
	if (dc->get_data_dim() == 2)
	{
		vector<vector<float>> y = dc->get_2d();
		vector<float> tmpMax;
		for (unsigned int i = 0; i < y.size(); i++)
			for (unsigned int j = 0; j < y[0].size(); j++)
				if (i == 0 || y[i][j] > tmpMax[j])
					tmpMax[j] = y[i][j];
		DataChunk *out = new DataChunk1D();
		out->set_data(tmpMax);
		return out;
	}
	else { throw "Data dim not supported."; }


}

//Whole Model
Model::Model(const string &input_fname, bool verbose) :m_verbose(verbose) {
	load_weights(input_fname);
}

vector<float> Model::compute_output(DataChunk *dc)
{
	DataChunk *in = dc;
	DataChunk *out;
	for (unsigned int l = 0; l < m_layers.size(); l++)
	{
		out = m_layers[l]->compute_output(in);
		if (in != dc) delete in;
		in = out;
	}
	vector<float> output = out->get_1d();
	delete out;
	return output;
}

void Model::load_weights(const string &input_config) {
	if (m_verbose) cout << "Reading model from " << input_config << endl;
	ifstream fin(input_config.c_str());
	fin >> m_layers_cnt;
	string layer_type = "";
	int tmp_layerNo;
	int tmp_layerNumber;
	if (m_verbose) cout << "Layers count " << m_layers_cnt << endl;
	for (int layer = 0; layer < m_layers_cnt; layer++)
	{
		fin >> tmp_layerNo >> layer_type >> tmp_layerNumber;
		if (m_verbose) cout << "Layer " << tmp_layerNo << " " << layer_type << " " << tmp_layerNumber << endl;
		Layer *l = 0L;
		ifstream layerfile(layer_type + "_" + to_string(tmp_layerNumber) + ".param");
		if (layer_type == "Convolution1D")
			l = new LayerConv1D();
		else if (layer_type == "Dense")
			l = new LayerDense();
		else if (layer_type == "Activation")
			l = new LayerActivation();
		else if (layer_type == "Maxpooling")
			l = new LayerMaxPooling();
		else if (layer_type == "Embedding")
			l = new LayerEmbedding();
		if (l == 0L) {
			cout << "Layer is empty, maybe it is not defined? Cannot define network. " << endl;
			return;
		}
		l->load_weights(layerfile);
		m_layers.push_back(l);
		layerfile.close();
	}
	fin.close();
}
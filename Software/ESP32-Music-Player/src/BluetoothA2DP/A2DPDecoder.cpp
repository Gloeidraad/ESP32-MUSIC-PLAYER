// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2020 Phil Schatzmann

#include "A2DPDecoder.h"

#if A2DP_MANAGED_DECODER_SUPPORTED

#include "BluetoothA2DPSink.h"
#include "esp_log.h"

size_t A2DPDecoder::write_pcm(const uint8_t* data, size_t len) {
  if (len > 0) ccall_audio_data_callback(data, (uint32_t)len);
  return len;
}

bool A2DPAudioDecoder::add_decoder(A2DPDecoder& decoder) {
  if (find(decoder.codec_type()) != nullptr) {
    ESP_LOGW(BT_AV_TAG,
             "A2DPAudioDecoder: a decoder for codec type %d is already "
             "registered",
             (int)decoder.codec_type());
    return false;
  }
  decoders.push_back(&decoder);
  return true;
}

A2DPDecoder* A2DPAudioDecoder::find(esp_a2d_mct_t type) {
  for (A2DPDecoder* dec : decoders) {
    if (dec->codec_type() == type) return dec;
  }
  return nullptr;
}

bool A2DPAudioDecoder::apply_mcc(const esp_a2d_mcc_t* mcc) {
  close();
  if (mcc == nullptr) return false;
  active = find(mcc->type);
  if (active == nullptr) {
    ESP_LOGW(BT_AV_TAG, "A2DPAudioDecoder: no decoder registered for codec type %d",
             (int)mcc->type);
    return false;
  }
  if (!active->begin(*mcc)) {
    ESP_LOGE(BT_AV_TAG, "A2DPAudioDecoder: decoder begin() failed for codec type %d",
             (int)mcc->type);
    active = nullptr;
    return false;
  }
  ESP_LOGI(BT_AV_TAG, "A2DPAudioDecoder: using decoder for codec type %d",
           (int)mcc->type);
  return true;
}

void A2DPAudioDecoder::process(const uint8_t* data, size_t len) {
  if (active != nullptr) active->write(data, len);
}

void A2DPAudioDecoder::close() {
  if (active != nullptr) {
    active->end();
    active = nullptr;
  }
}

audio_tools::AudioInfo A2DPAudioDecoder::get_audio_info() {
  if (active == nullptr) return audio_tools::AudioInfo();
  return active->get_audio_info();
}

#endif  // A2DP_MANAGED_DECODER_SUPPORTED

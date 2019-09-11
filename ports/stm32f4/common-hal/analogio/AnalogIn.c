/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common-hal/analogio/AnalogIn.h"
#include "py/runtime.h"
#include "supervisor/shared/translate.h"

#include "common-hal/microcontroller/Pin.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_bus.h"

//mp_raise_ValueError(translate("U dun goofed"));

void common_hal_analogio_analogin_construct(analogio_analogin_obj_t* self,
        const mcu_pin_obj_t *pin) {

    //No ADC function on pin
    if (pin->adc == 0xff) {
        mp_raise_ValueError(translate("Pin does not have ADC capabilities"));
    }
    //TODO: add ADC traits to structure?

    LL_GPIO_SetPinMode(pin_port(pin->number), pin_mask(pin->number), LL_GPIO_MODE_ANALOG);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1); //TODO: conditional on ADC unit
    claim_pin(pin);
    self->pin = pin;
}

bool common_hal_analogio_analogin_deinited(analogio_analogin_obj_t *self) {
    return self->pin == mp_const_none;
}

void common_hal_analogio_analogin_deinit(analogio_analogin_obj_t *self) {
    if (common_hal_analogio_analogin_deinited(self)) {
        return;
    }
    reset_pin_number(self->pin->number);
    self->pin = mp_const_none;
}

uint16_t common_hal_analogio_analogin_get_value(analogio_analogin_obj_t *self) {
    // Something else might have used the ADC in a different way,
    // so we completely re-initialize it.

    //HAL Implementation
    ADC_HandleTypeDef AdcHandle;
    ADC_ChannelConfTypeDef sConfig;

    AdcHandle.Instance = ADC1;
    AdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
    AdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    AdcHandle.Init.ScanConvMode = DISABLE;
    AdcHandle.Init.ContinuousConvMode = ENABLE;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.NbrOfDiscConversion = 0;
    AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
    AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.NbrOfConversion = 1;
    AdcHandle.Init.DMAContinuousRequests = ENABLE;
    AdcHandle.Init.EOCSelection = DISABLE;

    sConfig.Channel = stm32_adc_channel(self->pin->adc);
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    sConfig.Offset = 0;

    HAL_ADC_ConfigChannel(&AdcHandle, &sConfig);

    HAL_ADC_Start(&AdcHandle);
    HAL_ADC_PollForConversion(&AdcHandle,1); //timeout in ms
    uint16_t uhADCxConvertedData = (uint16_t)HAL_ADC_GetValue(&AdcHandle);
    HAL_ADC_Stop(&AdcHandle);

    //LL Implementation
    // if (LL_ADC_IsEnabled(ADC1) == 0)
    // {
    //     LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    //     LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    //     LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    //     LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, stm32_adc_channel(self->pin->adc));
    //     //LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_4);

    //     LL_ADC_SetChannelSamplingTime(ADC1, stm32_adc_channel(self->pin->adc), LL_ADC_SAMPLINGTIME_56CYCLES);
    //     LL_ADC_EnableIT_OVR(ADC1);
    // }
    // LL_ADC_Enable(ADC1);
    // uint16_t uhADCxConvertedData = (__LL_ADC_DIGITAL_SCALE(LL_ADC_RESOLUTION_12B) + 1);
    // LL_ADC_REG_StartConversionSWStart(ADC1);
    // while (LL_ADC_IsActiveFlag_EOCS(ADC1) == 0) {}
    // /* Retrieve ADC conversion data */
    // /* (data scale corresponds to ADC resolution: 12 bits) */
    // uhADCxConvertedData = LL_ADC_REG_ReadConversionData12(ADC1);
    
    // // Shift the value to be 16 bit.
    return uhADCxConvertedData << 4;
}

float common_hal_analogio_analogin_get_reference_voltage(analogio_analogin_obj_t *self) {
    return 3.3f;
}

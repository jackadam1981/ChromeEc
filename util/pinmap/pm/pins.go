// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package pm

// Pin types
const (
	ADC = iota
	PWM
	I2C
	Input
	Output
	OutputOD
	OutputODL
)

type Pin struct {
	PinType int
	Pin     string
	Signal  string
	Enum    string
}

type Pins struct {
	Adc  []*Pin // Analogue to digital converters
	I2c  []*Pin // I2C busses
	Gpio []*Pin // GPIO pins
	Pwm  []*Pin // Pwm pins
}

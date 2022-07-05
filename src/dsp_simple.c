/* Nonoptimized DSP loop */
/* wave table interpolation */
for (dsp_i = dsp_start; dsp_i < dsp_end; dsp_i++) {

	dsp_coeff = &interp_coeff[fluid_phase_fract_to_tablerow (dsp_phase)];
	dsp_phase_index = fluid_phase_index (dsp_phase);
	dsp_sample = (dsp_amp * (dsp_coeff->a0 * dsp_data[dsp_phase_index]
													 + dsp_coeff->a1 * dsp_data[dsp_phase_index + 1]
													 + dsp_coeff->a2 * dsp_data[dsp_phase_index + 2]
													 + dsp_coeff->a3 * dsp_data[dsp_phase_index + 3]));

	/* increment phase and amplitude */
	fluid_phase_incr (dsp_phase, dsp_phase_incr);
	dsp_amp += dsp_amp_incr;

	/* filter */
	/* The filter is implemented in Direct-II form. */
	dsp_centernode = dsp_sample - dsp_a1 * dsp_hist1 - dsp_a2 * dsp_hist2;
	dsp_sample =
		dsp_b0 * dsp_centernode + dsp_b1 * dsp_hist1 + dsp_b2 * dsp_hist2;
	dsp_hist2 = dsp_hist1;
	dsp_hist1 = dsp_centernode;

	/* pan */
	dsp_left_buf[dsp_i] += voice->amp_left * dsp_sample;
	dsp_right_buf[dsp_i] += voice->amp_right * dsp_sample;

	/* reverb */
	if (dsp_reverb_buf) 
		dsp_reverb_buf[dsp_i] += voice->amp_reverb * dsp_sample;

	/* chorus */
	if (dsp_chorus_buf) 
		dsp_chorus_buf[dsp_i] += voice->amp_chorus * dsp_sample;
}

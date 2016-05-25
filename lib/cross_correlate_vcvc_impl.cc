/* -*- c++ -*- */
/*
 * Copyright 2016 <+YOU OR YOUR COMPANY+>.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "cross_correlate_vcvc_impl.h"
#include <volk/volk.h>


namespace gr {
  namespace radar {

    cross_correlate_vcvc::sptr
    cross_correlate_vcvc::make(int vlen)
    {
      return gnuradio::get_initial_sptr
        (new cross_correlate_vcvc_impl(vlen));
    }

    /*
     * The private constructor
     */
    cross_correlate_vcvc_impl::cross_correlate_vcvc_impl(int vlen)
      : gr::sync_block("cross_correlate_vcvc",
                      gr::io_signature::make(2, 2, sizeof(gr_complex) * vlen),
                      gr::io_signature::make(1, 1, sizeof(gr_complex) * vlen))
    {
      // Verify datatype
      if(sizeof(gr_complex) != sizeof(fftwf_complex))
        std::runtime_error("sizeof(gr_complex) != sizeof(fftwf_complex)");
      d_vlen = vlen;
      d_fft_buffer1 = (gr_complex*) fftwf_malloc(sizeof(gr_complex) * d_vlen);
      d_fft_buffer2 = (gr_complex*) fftwf_malloc(sizeof(gr_complex) * d_vlen);
      d_out_buffer = (gr_complex*) fftwf_malloc(sizeof(gr_complex) * d_vlen);
      d_fft_plan1 = fftwf_plan_dft_1d(d_vlen, reinterpret_cast<fftwf_complex*>(d_fft_buffer1), reinterpret_cast<fftwf_complex*>(d_fft_buffer1),
                                      FFTW_FORWARD, FFTW_ESTIMATE);
      d_fft_plan2 = fftwf_plan_dft_1d(d_vlen, reinterpret_cast<fftwf_complex*>(d_fft_buffer2), reinterpret_cast<fftwf_complex*>(d_fft_buffer2),
                                      FFTW_FORWARD, FFTW_ESTIMATE);
      d_ifft_plan = fftwf_plan_dft_1d(d_vlen, (fftwf_complex*) d_out_buffer,
                    (fftwf_complex*) d_out_buffer, FFTW_BACKWARD, FFTW_ESTIMATE);

      // Correct volk alignment
      const int alignment_multiple = volk_get_alignment() / sizeof(gr_complex);
      set_alignment(std::max(1, alignment_multiple));
    }

    /*
     * Our virtual destructor.
     */
    cross_correlate_vcvc_impl::~cross_correlate_vcvc_impl()
    {
      fftwf_destroy_plan(d_fft_plan1);
      fftwf_destroy_plan(d_fft_plan2);
      fftwf_destroy_plan(d_ifft_plan);
      fftwf_free(d_fft_buffer1);
      fftwf_free(d_fft_buffer2);
      fftwf_free(d_out_buffer);
    }

    int
    cross_correlate_vcvc_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      // Cross correlation is defined as ifft(conj(fft(a))*fft(b))
      const gr_complex *in1 = (const gr_complex *) input_items[0];
      const gr_complex *in2 = (const gr_complex *) input_items[1];
      gr_complex *out = (gr_complex *) output_items[0];

      memcpy(d_fft_buffer1, in1, d_vlen*sizeof(gr_complex));
      memcpy(d_fft_buffer2, in2, d_vlen*sizeof(gr_complex));

      // Move to frequency domain
      fftwf_execute(d_fft_plan1);
      fftwf_execute(d_fft_plan2);
      int noi = d_vlen*noutput_items;

      // Calculate cross correlation
      // d_fft_buffer1 is conjugated
      // NOTE: Without zeropadding ffts, this will be a circular cross correlation
      for (int i = 0; i < d_vlen; i++) {
        d_out_buffer[i] = d_fft_buffer2[i] * std::conj(d_fft_buffer1[i]);
        std::cout << "loop, i: " << i << " result: " << d_out_buffer[i] << std::endl;
      }
      //volk_32fc_x2_multiply_conjugate_32fc( (lv_32fc_t*) d_out_buffer, (lv_32fc_t*) d_fft_buffer2,
      //                    (lv_32fc_t*)   d_fft_buffer1, noi);

      // Move back to time domain
      fftwf_execute(d_ifft_plan);
      // FFTW iift scales output by vlen
      // Let's normalize it (otherwise result is multiplied by vlen)
      volk_32fc_s32fc_multiply_32fc((lv_32fc_t*) d_out_buffer, (lv_32fc_t*) d_out_buffer, (lv_32fc_t) (1/(float)d_vlen), d_vlen);
      for (int i = 0; i < d_vlen; i++) {
        std::cout << "d_out_buffer, i: " << i << " result: " << d_out_buffer[i] << std::endl;
        ///out[i] = d_out_buffer[i] / vlen;

      }
      memcpy(out, d_out_buffer, d_vlen*sizeof(gr_complex));


      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace radar */
} /* namespace gr */

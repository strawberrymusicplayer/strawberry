/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2004, Melchior FRANZ <mfranz@kde.org>
   Copyright 2010, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
   Copyright 2017, Santiago Gil

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fht.h"

#include <algorithm>
#include <cmath>

#include <QList>
#include <QtMath>

FHT::FHT(uint n) : num_((n < 3) ? 0 : 1 << n), exp2_((n < 3) ? -1 : static_cast<int>(n)) {

  if (n > 3) {
    buf_vector_.resize(num_);
    tab_vector_.resize(num_ * 2);
    makeCasTable();
  }

}

FHT::~FHT() = default;

int FHT::sizeExp() const { return exp2_; }
int FHT::size() const { return num_; }

float *FHT::buf_() { return buf_vector_.data(); }
float *FHT::tab_() { return tab_vector_.data(); }
int *FHT::log_() { return log_vector_.data(); }

void FHT::makeCasTable() {

  float *costab = tab_();
  float *sintab = tab_() + num_ / 2 + 1;

  for (int ul = 0; ul < num_; ul++) {
    double d = M_PI * static_cast<double>(ul) / (static_cast<double>(num_) / 2.0);
    *costab = *sintab = static_cast<float>(cos(d));

    costab += 2;
    sintab += 2;
    if (sintab > tab_() + num_ * 2) sintab = tab_() + 1;
  }

}

void FHT::scale(float *p, float d) const {
  for (int i = 0; i < (num_ / 2); i++) *p++ *= d;
}

void FHT::ewma(float *d, float *s, float w) const {
  for (int i = 0; i < (num_ / 2); i++, d++, s++) *d = *d * w + *s * (1 - w);
}

void FHT::logSpectrum(float *out, float *p) {

  int n = num_ / 2, i = 0, k = 0, *r = nullptr;
  if (log_vector_.size() < n) {
    log_vector_.resize(n);
    float f = static_cast<float>(n) / static_cast<float>(log10(static_cast<double>(n)));
    for (i = 0, r = log_(); i < n; i++, r++) {
      int j = static_cast<int>(rint(log10(i + 1.0) * f));
      *r = j >= n ? n - 1 : j;
    }
  }
  semiLogSpectrum(p);
  *out++ = *p = *p / 100;
  for (k = i = 1, r = log_(); i < n; i++) {
    int j = *r++;
    if (i == j) {
      *out++ = p[i];
    }
    else {
      float base = p[k - 1];
      float step = (p[j] - base) / static_cast<float>(j - (k - 1));
      for (float corr = 0; k <= j; k++, corr += step) *out++ = base + corr;
    }
  }

}

void FHT::semiLogSpectrum(float *p) {

  power2(p);
  for (int i = 0; i < (num_ / 2); i++, p++) {
    float e = 10.0F * static_cast<float>(log10(sqrt(*p / static_cast<float>(2))));
    *p = e < 0 ? 0 : e;
  }

}

void FHT::spectrum(float *p) {

  power2(p);
  for (int i = 0; i < (num_ / 2); i++, p++) {
    *p = static_cast<float>(sqrt(*p / 2));
  }

}

void FHT::power(float *p) {

  power2(p);
  for (int i = 0; i < (num_ / 2); i++) *p++ /= 2;

}

void FHT::power2(float *p) {

  _transform(p, num_, 0);

  *p = static_cast<float>(2 * pow(*p, 2));
  p++;

  float *q = p + num_ - 2;
  for (int i = 1; i < (num_ / 2); i++) {
    *p = static_cast<float>(pow(*p, 2) + pow(*q, 2));
    p++;
    q--;
  }

}

void FHT::transform(float *p) {

  if (num_ == 8) {
    transform8(p);
  }
  else {
    _transform(p, num_, 0);
  }

}

void FHT::transform8(float *p) {

  float a = 0.0, b = 0.0, c = 0.0, d = 0.0, e = 0.0, f = 0.0, g = 0.0, h = 0.0, b_f2 = 0.0, d_h2 = 0.0;
  float a_c_eg = 0.0, a_ce_g = 0.0, ac_e_g = 0.0, aceg = 0.0, b_df_h = 0.0, bdfh = 0.0;

  a = *p++, b = *p++, c = *p++, d = *p++;
  e = *p++, f = *p++, g = *p++, h = *p;
  b_f2 = (b - f) * static_cast<float>(M_SQRT2);
  d_h2 = (d - h) * static_cast<float>(M_SQRT2);

  a_c_eg = a - c - e + g;
  a_ce_g = a - c + e - g;
  ac_e_g = a + c - e - g;
  aceg = a + c + e + g;

  b_df_h = b - d + f - h;
  bdfh = b + d + f + h;

  *p = a_c_eg - d_h2;
  *--p = a_ce_g - b_df_h;
  *--p = ac_e_g - b_f2;
  *--p = aceg - bdfh;
  *--p = a_c_eg + d_h2;
  *--p = a_ce_g + b_df_h;
  *--p = ac_e_g + b_f2;
  *--p = aceg + bdfh;

}

void FHT::_transform(float *p, int n, int k) {

  if (n == 8) {
    transform8(p + k);
    return;
  }

  int i = 0, j = 0, ndiv2 = n / 2;
  float a = 0.0, *t1 = nullptr, *t2 = nullptr, *t3 = nullptr, *t4 = nullptr, *ptab = nullptr, *pp = nullptr;

  for (i = 0, t1 = buf_(), t2 = buf_() + ndiv2, pp = &p[k]; i < ndiv2; i++)
    *t1++ = *pp++, *t2++ = *pp++;

  std::copy(buf_(), buf_() + n, p + k);

  _transform(p, ndiv2, k);
  _transform(p, ndiv2, k + ndiv2);

  j = num_ / ndiv2 - 1;
  t1 = buf_();
  t2 = t1 + ndiv2;
  t3 = p + k + ndiv2;
  ptab = tab_();
  pp = p + k;

  a = *ptab++ * *t3++;
  a += *ptab * *pp;
  ptab += j;

  *t1++ = *pp + a;
  *t2++ = *pp++ - a;

  for (i = 1, t4 = p + k + n; i < ndiv2; i++, ptab += j) {
    a = *ptab++ * *t3++;
    a += *ptab * *--t4;

    *t1++ = *pp + a;
    *t2++ = *pp++ - a;
  }

  std::copy(buf_(), buf_() + n, p + k);

}

/*

   Copyright (c) 2011, The Chinese University of Hong Kong

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#pragma once
#ifndef IDOCK_ATOM_HPP
#define IDOCK_ATOM_HPP

#include "vec3.hpp"

// AutoDock4 atom types.
const size_t AD_TYPE_H    =  0;	///< Non-polar hydrogen, i.e. bonded to carbon.
const size_t AD_TYPE_HD   =  1;	///< Polar hydrogen, i.e. bonded to a hetero atom.
const size_t AD_TYPE_C    =  2; ///< Carbon, not in a ring.
const size_t AD_TYPE_A    =  3; ///< Carbon, in a ring.
const size_t AD_TYPE_N    =  4; ///< Nitrogen, not a hydrogen bond acceptor.
const size_t AD_TYPE_NA   =  5; ///< Nitrogen, a hydrogen bond acceptor.
const size_t AD_TYPE_OA   =  6; ///< Oxygen, a hydrogen bond acceptor.
const size_t AD_TYPE_S    =  7; ///< Sulfur, not a hydrogen bond acceptor.
const size_t AD_TYPE_SA   =  8; ///< Sulfur, a hydrogen bond acceptor.
const size_t AD_TYPE_Se   =  9; ///< Selenium.
const size_t AD_TYPE_P    = 10; ///< Phosphorus.
const size_t AD_TYPE_F    = 11; ///< Fluorine.
const size_t AD_TYPE_Cl   = 12; ///< Chlorine.
const size_t AD_TYPE_Br   = 13; ///< Bromine.
const size_t AD_TYPE_I    = 14; ///< Iodine.
const size_t AD_TYPE_Zn   = 15; ///< Zine.
const size_t AD_TYPE_Fe   = 16; ///< Iron.
const size_t AD_TYPE_Mg   = 17; ///< Magnesium.
const size_t AD_TYPE_Ca   = 18; ///< Calcium.
const size_t AD_TYPE_Mn   = 19; ///< Manganese.
const size_t AD_TYPE_Cu   = 20; ///< Copper.
const size_t AD_TYPE_Na   = 21; ///< Sodium.
const size_t AD_TYPE_K    = 22; ///< Potassium.
const size_t AD_TYPE_Hg   = 23; ///< Mercury.
const size_t AD_TYPE_Ni   = 24; ///< Nickel.
const size_t AD_TYPE_Co   = 25; ///< Cobalt.
const size_t AD_TYPE_Cd   = 26; ///< Cadmium.
const size_t AD_TYPE_As   = 27; ///< Arsenic.
const size_t AD_TYPE_Sr   = 28; ///< Strontium.
const size_t AD_TYPE_SIZE = 29; ///< Number of supported AutoDock4 atom types.

const string ad_type_strings[] = ///< AutoDock4 atom type names.
{
	"H" , //  0 = AD_TYPE_H
	"HD", //  1 = AD_TYPE_HD
	"C" , //  2 = AD_TYPE_C
	"A" , //  3 = AD_TYPE_A
	"N" , //  4 = AD_TYPE_N
	"NA", //  5 = AD_TYPE_NA
	"OA", //  6 = AD_TYPE_OA
	"S" , //  7 = AD_TYPE_S
	"SA", //  8 = AD_TYPE_SA
	"Se", //  9 = AD_TYPE_Se
	"P" , // 10 = AD_TYPE_P
	"F" , // 11 = AD_TYPE_F
	"Cl", // 12 = AD_TYPE_Cl
	"Br", // 13 = AD_TYPE_Br
	"I" , // 14 = AD_TYPE_I
	"Zn", // 15 = AD_TYPE_Zn
	"Fe", // 16 = AD_TYPE_Fe
	"Mg", // 17 = AD_TYPE_Mg
	"Ca", // 18 = AD_TYPE_Ca
	"Mn", // 19 = AD_TYPE_Mn
	"Cu", // 20 = AD_TYPE_Cu
	"Na", // 21 = AD_TYPE_Na
	"K" , // 22 = AD_TYPE_K
	"Hg", // 23 = AD_TYPE_Hg
	"Ni", // 24 = AD_TYPE_Ni
	"Co", // 25 = AD_TYPE_Co
	"Cd", // 26 = AD_TYPE_Cd
	"As", // 27 = AD_TYPE_As
	"Sr", // 28 = AD_TYPE_Sr
};

/// Parses AutoDock4 atom type name, and returns AD_TYPE_SIZE if it does not match any supported AutoDock4 atom types.
inline size_t parse_ad_type_string(const string& ad_type_string)
{
	for (size_t i = 0; i < AD_TYPE_SIZE; ++i)
	{
		if (ad_type_strings[i] == ad_type_string) return i;
	}
	return AD_TYPE_SIZE;
}

// http://en.wikipedia.org/wiki/Atomic_radii_of_the_elements_(data_page)
// http://en.wikipedia.org/wiki/Covalent_radius
// The above two references have inconsistent values for covalent radius.
// The following definitions use the first reference, while OpenBabel uses the second.
const fl ad_covalent_radii[] = ///< AutoDock4 covalent radii, factorized by 1.1 for extra allowance.
{
	0.407, //  0 = AD_TYPE_H , 0.407 = 1.1 * 0.37
	0.407, //  1 = AD_TYPE_HD, 0.407 = 1.1 * 0.37
	0.847, //  2 = AD_TYPE_C , 0.847 = 1.1 * 0.77
	0.847, //  3 = AD_TYPE_A , 0.847 = 1.1 * 0.77
	0.825, //  4 = AD_TYPE_N , 0.825 = 1.1 * 0.75
	0.825, //  5 = AD_TYPE_NA, 0.825 = 1.1 * 0.75
	0.803, //  6 = AD_TYPE_OA, 0.803 = 1.1 * 0.73
	1.122, //  7 = AD_TYPE_S , 1.122 = 1.1 * 1.02
	1.122, //  8 = AD_TYPE_SA, 1.122 = 1.1 * 1.02
	1.276, //  9 = AD_TYPE_Se, 1.276 = 1.1 * 1.16
	1.166, // 10 = AD_TYPE_P , 1.166 = 1.1 * 1.06
	0.781, // 11 = AD_TYPE_F , 0.781 = 1.1 * 0.71
	1.089, // 12 = AD_TYPE_Cl, 1.089 = 1.1 * 0.99
	1.254, // 13 = AD_TYPE_Br, 1.254 = 1.1 * 1.14
	1.463, // 14 = AD_TYPE_I , 1.463 = 1.1 * 1.33
	1.441, // 15 = AD_TYPE_Zn, 1.441 = 1.1 * 1.31
	1.375, // 16 = AD_TYPE_Fe, 1.375 = 1.1 * 1.25
	1.430, // 17 = AD_TYPE_Mg, 1.430 = 1.1 * 1.30
	1.914, // 18 = AD_TYPE_Ca, 1.914 = 1.1 * 1.74
	1.529, // 19 = AD_TYPE_Mn, 1.529 = 1.1 * 1.39
	1.518, // 20 = AD_TYPE_Cu, 1.518 = 1.1 * 1.38
	1.694, // 21 = AD_TYPE_Na, 1.694 = 1.1 * 1.54
	2.156, // 22 = AD_TYPE_K , 2.156 = 1.1 * 1.96
	1.639, // 23 = AD_TYPE_Hg, 1.639 = 1.1 * 1.49
	1.331, // 24 = AD_TYPE_Ni, 1.331 = 1.1 * 1.21
	1.386, // 25 = AD_TYPE_Co, 1.386 = 1.1 * 1.26
	1.628, // 26 = AD_TYPE_Cd, 1.628 = 1.1 * 1.48
	1.309, // 27 = AD_TYPE_As, 1.309 = 1.1 * 1.19
	2.112, // 28 = AD_TYPE_Sr, 2.112 = 1.1 * 1.92
};

/// Returns covalent radius from an AutoDock4 atom type.
inline fl ad_covalent_radius(const size_t ad)
{
	return ad_covalent_radii[ad];
}

// XScore atom types.
const size_t XS_TYPE_C_H   =  0; ///< Carbon, hydrophobic, not bonded to a hetero atom.
const size_t XS_TYPE_C_P   =  1; ///< Carbon, bonded to a hetero atom.
const size_t XS_TYPE_N_P   =  2; ///< Nitrogen, neither hydrogen bond donor nor acceptor.
const size_t XS_TYPE_N_D   =  3; ///< Nitrogen, hydrogen bond donor.
const size_t XS_TYPE_N_A   =  4; ///< Nitrogen, hydrogen bond acceptor.
const size_t XS_TYPE_N_DA  =  5; ///< Nitrogen, both hydrogen bond donor and acceptor.
const size_t XS_TYPE_O_A   =  6; ///< Oxygen, hydrogen bond acceptor.
const size_t XS_TYPE_O_DA  =  7; ///< Oxygen, both hydrogen bond donor and acceptor.
const size_t XS_TYPE_S_P   =  8; ///< Sulfur or Selenium.
const size_t XS_TYPE_P_P   =  9; ///< Phosphorus.
const size_t XS_TYPE_F_H   = 10; ///< Fluorine, hydrophobic.
const size_t XS_TYPE_Cl_H  = 11; ///< Chlorine, hydrophobic.
const size_t XS_TYPE_Br_H  = 12; ///< Bromine, hydrophobic.
const size_t XS_TYPE_I_H   = 13; ///< Iodine, hydrophobic.
const size_t XS_TYPE_Met_D = 14; ///< Metal, hydrogen bond donor.
const size_t XS_TYPE_SIZE  = 15; ///< Number of supported XScore atom types.

const fl xs_vdw_radii[] = ///< Van der Waals radii for XScore atom types.
{
	1.9, //  0 = XS_TYPE_C_H
	1.9, //  1 = XS_TYPE_C_P
	1.8, //  2 = XS_TYPE_N_P
	1.8, //  3 = XS_TYPE_N_D
	1.8, //  4 = XS_TYPE_N_A
	1.8, //  5 = XS_TYPE_N_DA
	1.7, //  6 = XS_TYPE_O_A
	1.7, //  7 = XS_TYPE_O_DA
	2.0, //  8 = XS_TYPE_S_P
	2.1, //  9 = XS_TYPE_P_P
	1.5, // 10 = XS_TYPE_F_H
	1.8, // 11 = XS_TYPE_Cl_H
	2.0, // 12 = XS_TYPE_Br_H
	2.2, // 13 = XS_TYPE_I_H
	1.2  // 14 = XS_TYPE_Met_D
};

/// Returns Van der Waals radius from an XScore atom type.
inline fl xs_vdw_radius(const size_t xs)
{
	BOOST_ASSERT(xs < XS_TYPE_SIZE);
	return xs_vdw_radii[xs];
}

/// Returns true if the XScore atom type is hydrophobic.
inline bool xs_is_hydrophobic(const size_t xs)
{
	BOOST_ASSERT(xs < XS_TYPE_SIZE);
	return xs == XS_TYPE_C_H
		|| xs == XS_TYPE_F_H
		|| xs == XS_TYPE_Cl_H
		|| xs == XS_TYPE_Br_H
		|| xs == XS_TYPE_I_H;
}

/// Returns true if the XScore atom type is a hydrogen bond donor.
inline bool xs_is_donor(const size_t xs)
{
	BOOST_ASSERT(xs < XS_TYPE_SIZE);
	return xs == XS_TYPE_N_D
		|| xs == XS_TYPE_N_DA
		|| xs == XS_TYPE_O_DA
		|| xs == XS_TYPE_Met_D;
}

/// Returns true if the XScore atom type is a hydrogen bond acceptor.
inline bool xs_is_acceptor(const size_t xs)
{
	BOOST_ASSERT(xs < XS_TYPE_SIZE);
	return xs == XS_TYPE_N_A
		|| xs == XS_TYPE_N_DA
		|| xs == XS_TYPE_O_A
		|| xs == XS_TYPE_O_DA;
}

/// Returns true if the XScore atom type is either a hydrogen bond donor or a hydrogen bond acceptor.
inline bool xs_is_donor_acceptor(const size_t xs)
{
	BOOST_ASSERT(xs < XS_TYPE_SIZE);
	return xs_is_donor(xs) || xs_is_acceptor(xs);
}

/// Returns true if the two XScore atom types are a pair of hydrogen bond donor and acceptor.
inline bool xs_hbond(const size_t xs1, const size_t xs2)
{
	return (xs_is_donor(xs1) && xs_is_acceptor(xs2))
		|| (xs_is_donor(xs2) && xs_is_acceptor(xs1));
}

/// Mapping from AutoDock4 atom type to XScore atom type.
const size_t ad_to_xs[] =
{
	0,             //  0 = AD_TYPE_H
	0,             //  1 = AD_TYPE_HD
	XS_TYPE_C_H,   //  2 = AD_TYPE_C
	XS_TYPE_C_H,   //  3 = AD_TYPE_A
	XS_TYPE_N_P,   //  4 = AD_TYPE_N
	XS_TYPE_N_A,   //  5 = AD_TYPE_NA
	XS_TYPE_O_A,   //  6 = AD_TYPE_OA
	XS_TYPE_S_P,   //  7 = AD_TYPE_S
	XS_TYPE_S_P,   //  8 = AD_TYPE_SA
	XS_TYPE_S_P,   //  9 = AD_TYPE_Se
	XS_TYPE_P_P,   // 10 = AD_TYPE_P
	XS_TYPE_F_H,   // 11 = AD_TYPE_F
	XS_TYPE_Cl_H,  // 12 = AD_TYPE_Cl
	XS_TYPE_Br_H,  // 13 = AD_TYPE_Br
	XS_TYPE_I_H,   // 14 = AD_TYPE_I
	XS_TYPE_Met_D, // 15 = AD_TYPE_Zn
	XS_TYPE_Met_D, // 16 = AD_TYPE_Fe
	XS_TYPE_Met_D, // 17 = AD_TYPE_Mg
	XS_TYPE_Met_D, // 18 = AD_TYPE_Ca
	XS_TYPE_Met_D, // 19 = AD_TYPE_Mn
	XS_TYPE_Met_D, // 20 = AD_TYPE_Cu
	XS_TYPE_Met_D, // 21 = AD_TYPE_Na
	XS_TYPE_Met_D, // 22 = AD_TYPE_K
	XS_TYPE_Met_D, // 23 = AD_TYPE_Hg
	XS_TYPE_Met_D, // 24 = AD_TYPE_Ni
	XS_TYPE_Met_D, // 25 = AD_TYPE_Co
	XS_TYPE_Met_D, // 26 = AD_TYPE_Cd
	XS_TYPE_Met_D, // 27 = AD_TYPE_As
	XS_TYPE_Met_D, // 28 = AD_TYPE_Sr
};

// RF-Score atom types.
const size_t RF_TYPE_C    = 0; ///< Carbon.
const size_t RF_TYPE_N    = 1; ///< Nitrogen.
const size_t RF_TYPE_O    = 2; ///< Oxygen.
const size_t RF_TYPE_S    = 3; ///< Sulfur.
const size_t RF_TYPE_P    = 4; ///< Phosphorus.
const size_t RF_TYPE_F    = 5; ///< Fluorine.
const size_t RF_TYPE_Cl   = 6; ///< Chlorine.
const size_t RF_TYPE_Br   = 7; ///< Bromine.
const size_t RF_TYPE_I    = 8; ///< Iodine.
const size_t RF_TYPE_SIZE = 9; ///< Number of supported RF-Score atom types.

/// Mapping from AutoDock4 atom type to RF-Score atom type.
const size_t ad_to_rf[] =
{
	RF_TYPE_SIZE, //  0 = AD_TYPE_H
	RF_TYPE_SIZE, //  1 = AD_TYPE_HD
	RF_TYPE_C,    //  2 = AD_TYPE_C
	RF_TYPE_C,    //  3 = AD_TYPE_A
	RF_TYPE_N,    //  4 = AD_TYPE_N
	RF_TYPE_N,    //  5 = AD_TYPE_NA
	RF_TYPE_O,    //  6 = AD_TYPE_OA
	RF_TYPE_S,    //  7 = AD_TYPE_S
	RF_TYPE_S,    //  8 = AD_TYPE_SA
	RF_TYPE_SIZE, //  9 = AD_TYPE_Se
	RF_TYPE_P,    // 10 = AD_TYPE_P
	RF_TYPE_F,    // 11 = AD_TYPE_F
	RF_TYPE_Cl,   // 12 = AD_TYPE_Cl
	RF_TYPE_Br,   // 13 = AD_TYPE_Br
	RF_TYPE_I,    // 14 = AD_TYPE_I
	RF_TYPE_SIZE, // 15 = AD_TYPE_Zn
	RF_TYPE_SIZE, // 16 = AD_TYPE_Fe
	RF_TYPE_SIZE, // 17 = AD_TYPE_Mg
	RF_TYPE_SIZE, // 18 = AD_TYPE_Ca
	RF_TYPE_SIZE, // 19 = AD_TYPE_Mn
	RF_TYPE_SIZE, // 20 = AD_TYPE_Cu
	RF_TYPE_SIZE, // 21 = AD_TYPE_Na
	RF_TYPE_SIZE, // 22 = AD_TYPE_K
	RF_TYPE_SIZE, // 23 = AD_TYPE_Hg
	RF_TYPE_SIZE, // 24 = AD_TYPE_Ni
	RF_TYPE_SIZE, // 25 = AD_TYPE_Co
	RF_TYPE_SIZE, // 26 = AD_TYPE_Cd
	RF_TYPE_SIZE, // 27 = AD_TYPE_As
	RF_TYPE_SIZE, // 28 = AD_TYPE_Sr
};

/// Distance requirement of forming hydrogen bonds.
const fl hbond_dist_sqr = sqr(3.5);

/// Represents an atom by very simple fields.
class atom
{
public:
	string name; ///< Atom name;
	vec3 coordinate; ///< 3D coordinate.
	size_t ad; ///< AutoDock4 atom type.
	size_t xs; ///< XScore atom type.
	size_t rf; ///< RF-Score atom type.

	/// Constructs an atom with 3D coordinate and AutoDock4 atom type.
	explicit atom(string&& name_, const vec3& coordinate, const size_t ad) : name(static_cast<string&&>(name_)), coordinate(coordinate), ad(ad), xs(ad_to_xs[ad]), rf(ad_to_rf[ad]) {}

	/// Returns the covalent radius of current AutoDock4 atom type.
	fl covalent_radius() const
	{
		return ad_covalent_radius(ad);
	}

	/// Returns true if the atom is hydrogen.
	bool is_hydrogen() const
	{
		return (ad == AD_TYPE_H) || (ad == AD_TYPE_HD);
	}

	/// Returns true if the atom is a hetero atom, i.e. non-carbon heavy atom.
	bool is_hetero() const
	{
		return ad >= AD_TYPE_N;
	}

	/// Returns true if the current atom is covalently bonded to a given atom.
	bool is_neighbor(const atom& a) const
	{
		BOOST_ASSERT(this != &a);
		return (distance_sqr(coordinate, a.coordinate) < sqr(covalent_radius() + a.covalent_radius()));
	}

	/// For nitrogen and oxygen, revises the XScore atom type to make it a hydrogen bond donor.
	void donorize()
	{
		switch (xs)
		{
			case XS_TYPE_N_P : xs = XS_TYPE_N_D;  break;
			case XS_TYPE_N_A : xs = XS_TYPE_N_DA; break;
			case XS_TYPE_O_A : xs = XS_TYPE_O_DA; break;
		}
	}

	/// For carbon, revises the XScore atom type to make it non-hydrophobic.
	void dehydrophobicize()
	{
		BOOST_ASSERT(!is_hetero());
		xs = XS_TYPE_C_P;
	}
};

#endif

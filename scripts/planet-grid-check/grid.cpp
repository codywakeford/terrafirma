// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terragen/sphere.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using Vec = terragen::Sphere::Vec;

namespace {

Vec sub(Vec a, Vec b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec mul(Vec a,double k) { return {a.x*k,a.y*k,a.z*k}; }
double dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec cross(Vec a, Vec b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
double length(Vec a) { return std::sqrt(dot(a,a)); }
Vec unit(Vec a) { return mul(a,1/length(a)); }

enum class Mapping { Equiangular, Spherified, Cobe, Smooth25, Smooth50, Smooth75, Locked50, Locked65, SquareCentre };

const char *name(Mapping mapping)
{
	return mapping==Mapping::Equiangular ? "equiangular" :
		mapping==Mapping::Spherified ? "spherified" :
		mapping==Mapping::Cobe ? "cobe" :
		mapping==Mapping::Smooth25 ? "smooth-25" :
		mapping==Mapping::Smooth50 ? "smooth-50" :
		mapping==Mapping::Smooth75 ? "smooth-75" :
		mapping==Mapping::Locked50 ? "locked-50" :
		mapping==Mapping::Locked65 ? "locked-65" : "square-centre";
}

double parameter(Mapping mapping,double t)
{
	if(mapping!=Mapping::SquareCentre && mapping!=Mapping::Locked50 && mapping!=Mapping::Locked65) return t;
	// A symmetric monotonic coordinate with metre-scale cells at the face
	// centre for the existing radius, while retaining fixed face edges.
	constexpr double a=M_PI*std::sqrt(2.0)/4;
	return a*t+(1-a)*t*t*t;
}

Vec spherify(Vec c)
{
	const double x2=c.x*c.x,y2=c.y*c.y,z2=c.z*c.z;
	return {c.x*std::sqrt(1-y2/2-z2/2+y2*z2/3),
		c.y*std::sqrt(1-z2/2-x2/2+z2*x2/3),
		c.z*std::sqrt(1-x2/2-y2/2+x2*y2/3)};
}

double cobe(double a,double b)
{
	constexpr double lambda=0.7240,g10=-0.0941,g01=0.0276,g20=-0.0623,g11=0.0409,g02=0.0342;
	const double a2=a*a,b2=b*b;
	const double correction=g10*a2+g01*b2+g20*a2*a2+g11*a2*b2+g02*b2*b2;
	return lambda*a+(1-lambda)*a*a2+(1-a2)*a*correction;
}

Vec point(Mapping mapping,int half,int face,double x,double z)
{
	const double radius=4.0*half/M_PI;
	if(mapping==Mapping::Equiangular) {
		const terragen::Sphere sphere(half);
		return sphere.point({face,x,z},0);
	}
	const auto &axes=terragen::Sphere::axes(face);
	if(mapping==Mapping::Cobe) {
		const double a=x/half,b=z/half,u=cobe(a,b),v=cobe(b,a);
		return mul(unit({axes.out.x+u*axes.east.x+v*axes.north.x,
			axes.out.y+u*axes.east.y+v*axes.north.y,
			axes.out.z+u*axes.east.z+v*axes.north.z}),radius);
	}
	const double u=parameter(mapping,x/half),v=parameter(mapping,z/half);
	const Vec cube{axes.out.x+u*axes.east.x+v*axes.north.x,
		axes.out.y+u*axes.east.y+v*axes.north.y,
		axes.out.z+u*axes.east.z+v*axes.north.z};
	const Vec rounded=spherify(cube);
	if(mapping==Mapping::Spherified || mapping==Mapping::SquareCentre) return mul(rounded,radius);
	const double a=M_PI*x/(4*half),b=M_PI*z/(4*half);
	const Vec gnomonic=unit({axes.out.x+std::tan(a)*axes.east.x+std::tan(b)*axes.north.x,
		axes.out.y+std::tan(a)*axes.east.y+std::tan(b)*axes.north.y,
		axes.out.z+std::tan(a)*axes.east.z+std::tan(b)*axes.north.z});
	if(mapping==Mapping::Locked50 || mapping==Mapping::Locked65) {
		const double inner=mapping==Mapping::Locked50?0.50:0.65;
		const double edge=std::max(std::abs(x),std::abs(z))/half;
		double weight=std::clamp((1-edge)/(1-inner),0.0,1.0);
		weight=weight*weight*(3-2*weight);
		return mul(unit({(1-weight)*gnomonic.x+weight*rounded.x,
			(1-weight)*gnomonic.y+weight*rounded.y,
			(1-weight)*gnomonic.z+weight*rounded.z}),radius);
	}
	const double weight=mapping==Mapping::Smooth25?0.25:mapping==Mapping::Smooth50?0.5:0.75;
	return mul(unit({(1-weight)*gnomonic.x+weight*rounded.x,
		(1-weight)*gnomonic.y+weight*rounded.y,
		(1-weight)*gnomonic.z+weight*rounded.z}),radius);
}

struct Cell {
	double shortest, longest, angle, area, jacobian;
};

Cell cell(Mapping mapping,int half,double x,double z)
{
	const Vec a=point(mapping,half,0,x,z), b=point(mapping,half,0,x+1,z);
	const Vec c=point(mapping,half,0,x,z+1), d=point(mapping,half,0,x+1,z+1);
	const Vec e=sub(b,a), n=sub(c,a), e2=sub(d,c), n2=sub(d,b);
	const double le=length(e), ln=length(n), le2=length(e2), ln2=length(n2);
	const double angle=std::acos(std::clamp(dot(e,n)/(le*ln),-1.0,1.0))*180/M_PI;
	const double area=(length(cross(e,n))+length(cross(n2,e2)))/2;
	const Vec up=unit(point(mapping,half,0,x+0.5,z+0.5));
	return {std::min({le,ln,le2,ln2}),std::max({le,ln,le2,ln2}),angle,area,-dot(cross(e,n),up)};
}

struct Range {
	double minSide=std::numeric_limits<double>::infinity(), maxSide=0;
	double minAngle=180, maxAngle=0, minArea=std::numeric_limits<double>::infinity(), maxArea=0;
	double minJacobian=std::numeric_limits<double>::infinity();
	double sumArea=0;
	int cells=0;
	void add(const Cell &c) {
		minSide=std::min(minSide,c.shortest); maxSide=std::max(maxSide,c.longest);
		minAngle=std::min(minAngle,c.angle); maxAngle=std::max(maxAngle,c.angle);
		minArea=std::min(minArea,c.area); maxArea=std::max(maxArea,c.area);
		minJacobian=std::min(minJacobian,c.jacobian); sumArea+=c.area; cells++;
	}
};

const char *region(double x, double z, int half)
{
	const bool ex=std::abs(x+0.5)>0.75*half, ez=std::abs(z+0.5)>0.75*half;
	return ex&&ez ? "corner" : ex||ez ? "edge" : "interior";
}

unsigned char byte(double value) { return (unsigned char)std::lround(255*std::clamp(value,0.0,1.0)); }

double seamError(Mapping mapping,int half)
{
	const terragen::Sphere sphere(half);
	double worst=0;
	for(int face=0;face<6;face++) for(int side=0;side<4;side++)
	for(int i=0;i<=32;i++) {
		const double along=-half+2.0*half*i/32;
		terragen::Sphere::Place edge{face,side<2?(side==0?double(half):-double(half)):along,
			side>=2?(side==2?double(half):-double(half)):along};
		terragen::Sphere::Place past=edge;
		if(side<2) past.x+=side==0?1e-7:-1e-7; else past.z+=side==2?1e-7:-1e-7;
		const auto there=sphere.settle(past);
		worst=std::max(worst,length(sub(point(mapping,half,edge.face,edge.x,edge.z),
			point(mapping,half,there.face,there.x,there.z))));
	}
	return worst;
}

void audit(Mapping mapping,int half,const std::filesystem::path &out,std::ofstream &summary)
{
	constexpr int pixels=128;
	std::array<Range,3> ranges;
	Range all;
	std::ofstream image(out/(std::string(name(mapping))+"-"+std::to_string(8*half)+".ppm"));
	image << "P3\n" << pixels << " " << pixels << "\n255\n";
	for (int j=0;j<pixels;j++) for (int i=0;i<pixels;i++) {
		const double x=-half+(2.0*half-1)*i/(pixels-1);
		const double z=-half+(2.0*half-1)*j/(pixels-1);
		const Cell c=cell(mapping,half,x,z);
		const std::string r=region(x,z,half);
		const int ri=r=="interior"?0:r=="edge"?1:2;
		ranges[ri].add(c);
		all.add(c);
		// Red: loss of a right angle. Green: side-length loss. Blue: area loss.
		image << int(byte(std::abs(c.angle-90)/30)) << ' '
			<< int(byte(1-c.shortest)) << ' ' << int(byte(1-c.area)) << '\n';
	}
	const double seam=seamError(mapping,half);
	for (int i=0;i<3;i++) {
		const Range &r=ranges[i];
		summary << name(mapping) << ',' << 8*half << ',' << (i==0?"interior":i==1?"edge":"corner") << ',' << r.cells << ','
			<< r.minSide << ',' << r.maxSide << ',' << r.minAngle << ',' << r.maxAngle << ','
			<< r.minArea << ',' << r.maxArea << ',' << r.sumArea/r.cells << ',' << r.minJacobian << ',' << seam << '\n';
	}
	summary << name(mapping) << ',' << 8*half << ",all," << all.cells << ',' << all.minSide << ',' << all.maxSide << ','
		<< all.minAngle << ',' << all.maxAngle << ',' << all.minArea << ',' << all.maxArea << ',' << all.sumArea/all.cells << ','
		<< all.minJacobian << ',' << seam << '\n';
}

std::array<Range,4> measure(Mapping mapping,int half)
{
	std::array<Range,4> ranges;
	constexpr int samples=128;
	for(int j=0;j<samples;j++) for(int i=0;i<samples;i++) {
		const double x=-half+(2.0*half-1)*i/(samples-1);
		const double z=-half+(2.0*half-1)*j/(samples-1);
		const Cell c=cell(mapping,half,x,z);
		const std::string r=region(x,z,half);
		ranges[r=="interior"?0:r=="edge"?1:2].add(c);
		ranges[3].add(c);
	}
	return ranges;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc!=2) { std::cerr << "usage: planet-grid-audit OUTPUT_DIR\n"; return 2; }
	const std::filesystem::path out=argv[1];
	std::filesystem::create_directories(out);
	std::ofstream summary(out/"summary.csv");
	summary << std::setprecision(9);
	summary << "mapping,circumference,region,cells,min_side,max_side,min_angle,max_angle,min_area,max_area,average_area,min_jacobian,seam_error\n";
	for (Mapping mapping : {Mapping::Equiangular,Mapping::Spherified,Mapping::Cobe,Mapping::Smooth25,Mapping::Smooth50,
			Mapping::Smooth75,Mapping::Locked50,Mapping::Locked65,Mapping::SquareCentre})
		for (int half : {256,512,1024,2048}) audit(mapping,half,out,summary);
	for(int half : {256,512,1024,2048}) {
		const auto old=measure(Mapping::Equiangular,half),candidate=measure(Mapping::Cobe,half);
		const auto angleError=[](const Range &r){return std::max(90-r.minAngle,r.maxAngle-90);};
		if(candidate[3].minJacobian<=old[3].minJacobian || angleError(candidate[0])>=angleError(old[0]) ||
				seamError(Mapping::Cobe,half)>1e-5) {
			std::cerr << "planet grid audit: COBE candidate failed its geometry gate at circumference " << 8*half << '\n';
			return 1;
		}
	}
	std::cout << "planet grid audit: " << out << "/summary.csv and thirty-six PPM heat maps\n";
}

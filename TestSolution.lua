workspace "NVRHI-Test"
	architecture "x86_64"
	configurations { "Debug", "Debug-AS", "Release", "Dist" }
	startproject "NVRHI"
    conformancemode "On"

	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	flags { "MultiProcessorCompile" }

	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

	include "premake5.lua"
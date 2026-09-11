project "premake5"
	kind "Utility"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/Tools/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/Tools/%{prj.name}")

	files
	{
		"%{wks.location}/**premake5.lua"
	}

	postbuildmessage "Regenerating project files with Premake5!"
	postbuildcommands
	{
		"\"%{prj.location}premake5\" %{_ACTION} --file=\"%{wks.location}premake5.lua\""
	}

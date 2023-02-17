from os import path 
import os

Import("env")

print("====== CUSTOM SCRIPT =====")

repo_url = env.GetProjectOption("repo_libssh2")
framework = env.GetProjectOption("framework")
frwdir = env.PioPlatform().get_package_dir(f"framework-{framework[0]}")
target_dir = path.join(frwdir, "components", "libssh2" )

if not path.exists(target_dir):
    print (f"patching {framework} located in {frwdir} with {repo_url}")
    env.Execute(f"git clone {repo_url} {target_dir}")
    #update CMakeLists and libssh2_config with own version
    env.Execute(f"cp $PROJECT_DIR/patches/CMakeLists.txt {target_dir}/CMakeLists.txt")
    env.Execute(f"cp $PROJECT_DIR/patches/libssh2_config.h {target_dir}/include/libssh2_config.h")

else:
    print (f"checking updates for {target_dir} in {repo_url}")
    print ("\t--> not implemented yet... ")

print("====== CUSTOM SCRIPT DONE =====")

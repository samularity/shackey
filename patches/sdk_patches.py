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
    env.Execute(f'git -C "{target_dir}"  pull --rebase {repo_url} 0834b9bcc85b90c78afff103f909b5a909b95e45')

else:
    print (f"checking updates for {target_dir} in {repo_url}")
    env.Execute(f'git -C "{target_dir}" reset --hard origin/master')
    #env.Execute(f'git -C "{target_dir}"  pull --rebase {repo_url}')
    env.Execute(f'git -C "{target_dir}"  pull --rebase {repo_url} 0834b9bcc85b90c78afff103f909b5a909b95e45')


#update CMakeLists and libssh2_config with own version
env.Execute(f"cp $PROJECT_DIR/patches/CMakeLists.txt {target_dir}/CMakeLists.txt")
env.Execute(f"cp $PROJECT_DIR/patches/libssh2_config.h {target_dir}/include/libssh2_config.h")

print("====== CUSTOM SCRIPT DONE =====")

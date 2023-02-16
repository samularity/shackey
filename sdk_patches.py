from os.path import join, isfile

Import("env")

#FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduinoavr")
#patchflag_path = join(FRAMEWORK_DIR, ".patching-done")

print("====== CUSTOM SCRIPT =====")
#print(env.GetProjectOption("framework"))
framework = env.GetProjectOption("framework")
print(framework)
frwdir = env.PioPlatform().get_package_dir(f"framework-{framework[0]}")
print(frwdir)
print (f"patching {framework} located in {frwdir}")

patchflag_path = join(frwdir, ".patching-done")

# patch file only if we didn't do it before
if not isfile(patchflag_path):
    #framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config.h
    original_file = join(frwdir, "components", "mbedtls" , "mbedtls", "include", "mbedtls", "config.h")
    patched_file = join("patches", "1-framework-mbedtls-esp32c3-include.patch")

    print (original_file)

    assert isfile(original_file) and isfile(patched_file)

    env.Execute("patch %s %s" % (original_file, patched_file))
    # env.Execute("touch " + patchflag_path)


    def _touch(path):
        with open(path, "w") as fp:
            fp.write("")

    env.Execute(lambda *args, **kwargs: _touch(patchflag_path))

print("====== CUSTOM SCRIPT DONE =====")
# Third-party notices

Qwertycoin incorporates third-party components under their respective licenses. The complete corresponding source and license texts are available at the exact source revision recorded in `BUILD-INFO.json`, including:

- RandomX (`external/randomx/LICENSE`)
- LMDB (`external/db_drivers/liblmdb/LICENSE`)
- RapidJSON (`external/rapidjson/license.txt` and bundled third-party licenses)
- epee (`contrib/epee/LICENSE.txt`)

Runtime libraries bundled for portability remain subject to their own upstream licenses. This notice supplements, and does not replace, the repository `LICENSE` or upstream notices.

Depending on the platform dependency closure, the package can include runtime components from Boost, OpenSSL, ZeroMQ, OpenPGM, Unbound, libsodium, libgcrypt, libgpg-error, Expat and the GCC/MinGW runtime. Their exact shipped filenames and hashes are recorded by the inner package manifest. License information is available from the corresponding upstream projects and from the package-manager records of the toolchain named in `BUILD-INFO.json`.

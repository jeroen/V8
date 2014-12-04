require 'formula'

class V83145 < Formula
  homepage 'http://code.google.com/p/v8/'
  url 'https://github.com/v8/v8/archive/3.14.5.tar.gz'
  sha1 '595492842ff86eaa7e8cf5cf88b2dd9a000e357f'

  keg_only 'Conflicts with V8 in main repository.'

  # gyp currently depends on a full xcode install
  # https://code.google.com/p/gyp/issues/detail?id=292
  depends_on :xcode

  def install
    system 'make dependencies'
    system 'make', 'native',
                   "-j#{ENV.make_jobs}",
                   "library=shared",
                   "snapshot=on",
                   "console=readline"

    prefix.install 'include'
    cd 'out/native' do
      lib.install Dir['lib*']
      bin.install 'd8', 'lineprocessor', 'mksnapshot', 'preparser', 'process', 'shell' => 'v8'
    end
  end
end

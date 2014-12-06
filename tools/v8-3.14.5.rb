require 'formula'

class V83145 < Formula
  homepage 'http://code.google.com/p/v8/'
  url 'https://github.com/v8/v8-git-mirror/archive/3.14.5.10.tar.gz'
  sha1 'd7308e3a17d278f30086f574b325ff3974311abb'

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

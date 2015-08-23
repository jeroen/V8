class V8315 < Formula
  homepage "https://code.google.com/p/v8/"
  # Use the official github mirror, it is easier to find tags there
  url "https://github.com/v8/v8/archive/3.15.11.tar.gz"
  sha1 "0c47b3a5409d71d4fd6581520c8972f7451a87e4"

  bottle do
    cellar :any
    sha1 "d2304e24d2fa6d6d499327d98755f4a0088d83d4" => :yosemite
    sha1 "1c3a4a0b45f0d5a706e048e80f5a497a6d2e02ec" => :mavericks
    sha1 "be82b18f5f267be5e11da92c62efb8a4c89f33c1" => :mountain_lion
  end

  keg_only "Conflicts with V8 in Homebrew/homebrew."

  def install
    system "make", "dependencies"
    system "make", "native",
                   "-j#{ENV.make_jobs}",
                   "library=shared",
                   "snapshot=on",
                   "console=readline"

    prefix.install "include"
    cd "out/native" do
      lib.install Dir["lib*"]
      bin.install "d8", "lineprocessor", "mksnapshot", "preparser", "process", "shell" => "v8"
    end
  end
end

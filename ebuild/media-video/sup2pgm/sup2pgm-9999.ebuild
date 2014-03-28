EAPI=4
inherit eutils git-2

DESCRIPTION="Converts BluRay presentation graphics streams (SUP subtitles) to PGM images"
HOMEPAGE="https://github.com/ksa242/sup2pgm"
SRC_URI=""

EGIT_REPO_URI="git://github.com/ksa242/sup2pgm.git"

LICENSE="BSD-2"
SLOT="0"

RDEPEND=""
DEPEND=""

DOCS=( README )

src_install() {
	dobin sup2pgm || die
	dodoc README || die
}

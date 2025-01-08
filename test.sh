#!/bin/sh
BASEDIR=$(dirname "$(realpath $0)")

echo "TEST Adding Repo."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" add-repo file://$BASEDIR/example_repo
echo "TEST Update."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" update
echo "TEST Remove Repo."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" remove-repo io.github.kindlemodding_repo
echo "TEST Update."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" update
echo "TEST Adding Repo."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" add-repo file://$BASEDIR/example_repo
echo "TEST Update."
$BASEDIR/builddir/kpm -vd --kpm_dir "$BASEDIR/builddir/kpm_folder" update
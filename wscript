
#
# This file is the default set of rules to compile a Pebble project.
#
# Feel free to customize this to your needs.
#

try:
    from sh import jshint, ErrorReturnCode_2
    hint = jshint
except ImportError:
    hint = None

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')
    global hint
    if hint is not None:
        hint = hint.bake(['--config', 'pebble-jshintrc'])

def build(ctx):

    ctx.load('pebble_sdk')

    ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'),
                    target='pebble-app.elf')


LProfile = require('lprofile')

function a()
    print('running a')
    b()
    c()
    c()
end

function b()
    print('running b')
    love.timer.sleep(2)
end

function c()
    print('running c')
    b()
end

function love.load()
    print('starting')
    LProfile.start('callgrind.1')
    print('started')
    a()
    print('stopping')
    LProfile.stop()
    love.event.quit()
end


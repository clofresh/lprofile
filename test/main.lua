LProfile = require('lprofile')

function test()
    print('running function')
end

function love.load()
    print('starting')
    LProfile.start()
    print('started')
    test()
    print('stopping')
    LProfile.stop()
end


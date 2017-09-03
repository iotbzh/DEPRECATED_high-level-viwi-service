--[[
  Copyright (C) 2017 "IoT.bzh"
  Author Romain Forlot <romain.forlot@iot.bzh>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
--]]

function _SubscribeLowCan (request, args)

    AFB:notice ("_We gonna subscribe to %s", args)
    local err, result= AFB:servsync ("low-can","subscribe", args)
    if (err) then
        AFB:fail ("AFB:service_call_sync fail");
    else
        AFB:success (request, result["response"])
    end
end

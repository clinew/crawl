define(["jquery", "comm"], function ($, comm) {
    var new_message_count = 0;

    function update_spectators(data)
    {
        $("#spectator_count").html(data.count + " spectators");
        $("#spectator_list").html(data.names);
    }

    function receive_message(data)
    {
        var msg = data.content;
        $("#chat_history").append(msg + "<br>");
        $("#chat_history_container").scrollTop($("#chat_history").height());
        if ($("#chat_body").css("display") === "none")
        {
            new_message_count++;
            update_message_count();
        }
    }

    function update_message_count()
    {
        var press_key = new_message_count > 0 ? " (Press F12)" : "";
        $("#message_count").html(new_message_count + " new messages" + press_key);
        $("#message_count").toggleClass("has_new", new_message_count > 0);
    }

    function chat_message_send(e)
    {
        if (e.which == 13)
        {
            e.preventDefault();
            e.stopPropagation();
            comm.send_message("chat_msg", {
                text: $("#chat_input").val()
            });
            $("#chat_input").val("");
            return false;
        }
        else if (e.which == 27)
        {
            e.preventDefault();
            e.stopPropagation();
            toggle();
            $(document.activeElement).blur();
        }
        return true;
    }

    function toggle()
    {
        if ($("#chat_body").css("display") === "none")
        {
            $("#chat_body").slideDown(200);
            new_message_count = 0;
            update_message_count();
            $("#message_count").html("(Esc: back to game)");
        }
        else
        {
            $("#chat_body").slideUp(200);
            update_message_count();
        }
    }

    function focus()
    {
        if ($("#chat_body").css("display") === "none")
            toggle();

        $("#chat_history_container").scrollTop($("#chat_history").height());
        $("#chat_input").focus();
    }

    function clear()
    {
        $("#spectator_list").html("&nbsp;");
        $("#chat_history").html("");
        $("#spectator_count").html("0 spectators");
        new_message_count = 0;
        update_message_count();
        $("#chat_body").slideUp(200);
    }

    $(document).ready(function () {
        $("#chat_input").bind("keydown", chat_message_send);
        $("#chat_caption").bind("click", toggle);
    });

    comm.register_handlers({
        "chat": receive_message,
        "update_spectators": update_spectators
    });

    return {
        clear: clear,
        focus: focus,
    }
});
